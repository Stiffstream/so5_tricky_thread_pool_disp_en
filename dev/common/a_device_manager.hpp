#pragma once

#include <common/args.hpp>
#include <common/a_dashboard.hpp>

#include <random>

class a_device_manager_t final : public so_5::agent_t {
public:
   using clock_t = a_dashboard_t::clock_t;

   struct msg_base_t : public so_5::message_t {
      // Время, в которое ожидается прибытие сообщения.
      clock_t::time_point expected_time_;

      // Конструктор по умолчанию. В качестве времени ожидаемого
      // прибытия берется текущее время.
      msg_base_t()
         :  expected_time_(clock_t::now())
      {}
      // Инициализирующий конструктор. Ожидаемое время прибытия задается явно.
      msg_base_t(clock_t::time_point expected_time)
         :  expected_time_(expected_time)
      {}
   };

   // Описание одного устройства.
   // Этот объект будет создаваться при старте и затем будет
   // пересылаться внутри сообщений, относящихся к этому устройству.
   struct device_t final {
      using id_t = std::uint_fast64_t;
      // Уникальный идентификатор устройства.
      id_t id_;
      // Время между операциями ввода вывода для этого устройства.
      // Вычисляется заново при переинициализации устройства.
      std::chrono::milliseconds io_period_;
      // Количество оставшихся операций ввода вывода.
      // При достижении нуля инициируется операция переинициализации
      // устройства.
      unsigned remaining_io_ops_;
      // Количество оставшихся операций переинициализации устройства.
      unsigned remaining_reinits_;

      device_t(
            id_t id,
            std::chrono::milliseconds io_period,
            unsigned remaining_io_ops,
            unsigned remaining_reinits)
         :  id_(id)
         ,  io_period_(io_period)
         ,  remaining_io_ops_(remaining_io_ops)
         ,  remaining_reinits_(remaining_reinits)
      {}
   };

   using device_uptr_t = std::unique_ptr<device_t>;

   // Сообщение о необходимости инициализации нового устройства.
   struct init_device_t final : public msg_base_t {
      device_t::id_t id_;

      init_device_t(device_t::id_t id) : id_(id) {}
   };

   // Сообщение о необходимости переинициализации устройства.
   struct reinit_device_t final : public msg_base_t {
      device_uptr_t device_;

      reinit_device_t(device_uptr_t device) : device_(std::move(device)) {}
   };

   // Сообщение о необходимости выполнить IO-операцию на устройстве.
   struct perform_io_t final : public msg_base_t {
      device_uptr_t device_;

      perform_io_t(
         device_uptr_t device,
         clock_t::time_point expected_time)
         :  msg_base_t(expected_time)
         ,  device_(std::move(device))
      {}
   };

   a_device_manager_t(
         context_t ctx,
         const args_t & args,
         so_5::mbox_t dashboard_mbox)
         :  so_5::agent_t(std::move(ctx))
         ,  args_(args)
         ,  dashboard_mbox_(std::move(dashboard_mbox)) {
      so_subscribe_self()
         .event(&a_device_manager_t::on_init_device, so_5::thread_safe)
         .event(&a_device_manager_t::on_reinit_device, so_5::thread_safe)
         .event(&a_device_manager_t::on_perform_io, so_5::thread_safe);
   }

   virtual void so_evt_start() override {
      // Отсылаем сообщения для создания новых устройств в нужном количестве.
      device_t::id_t id{};
      for(unsigned i = 0; i != args_.device_count_; ++i, ++id)
         so_5::send<init_device_t>(*this, id);
   }

private:
   const args_t args_;
   const so_5::mbox_t dashboard_mbox_;

   void on_init_device(mhood_t<init_device_t> cmd) const {
      // Обновим статистику по этой операции.
      handle_msg_delay(a_dashboard_t::op_type_t::init, *cmd);

      // Нужно создать новое устройство и проимитировать паузу,
      // связанную с его инициализацией.
      auto dev = std::make_unique<device_t>(cmd->id_,
            calculate_io_period(),
            calculate_io_ops_before_reinit(),
            calculate_reinits_before_recreate());

      std::this_thread::sleep_for(args_.device_init_time_);

      // Отсылаем первое сообщение о необходимости выполнить IO-операцию
      // на этом устройстве.
      send_perform_io_msg(std::move(dev));
   }

   void on_reinit_device(mutable_mhood_t<reinit_device_t> cmd) const {
      // Обновим статистику по этой операции.
      handle_msg_delay(a_dashboard_t::op_type_t::reinit, *cmd);

      // Нужно обновить основные параметры устройства после переинициализации.
      cmd->device_->io_period_ = calculate_io_period();
      cmd->device_->remaining_io_ops_ = calculate_io_ops_before_reinit();
      cmd->device_->remaining_reinits_ -= 1;

      // Имитируем задержку операции переинициализации устройства.
      // Переинициализация выполняется 2/3 от времени инициализации.
      std::this_thread::sleep_for((args_.device_init_time_/3)*2);

      // Продолжаем выполнять IO-операции с этим устройством.
      send_perform_io_msg(std::move(cmd->device_));
   }

   void on_perform_io(mutable_mhood_t<perform_io_t> cmd) const {
      // Обновим статистику по этой операции.
      handle_msg_delay(a_dashboard_t::op_type_t::io_op, *cmd);

      // Выполняем задержку имитируя реальную IO-операцию.
      std::this_thread::sleep_for(args_.io_op_time_);

      // Количество оставшихся IO-операций должно уменьшиться.
      cmd->device_->remaining_io_ops_ -= 1;
      // Возможно, пришло время переинициализировать устройство.
      // Или даже пересоздавать, если исчерпан лимит попыток переинициализации.
      if(0 == cmd->device_->remaining_io_ops_) {
         if(0 == cmd->device_->remaining_reinits_)
            // Устройство нужно пересоздать. Под тем же самым идентификатором.
            so_5::send<init_device_t>(*this, cmd->device_->id_);
         else
            // Попытки переинициализации еще не исчерпаны.
            so_5::send<so_5::mutable_msg<reinit_device_t>>(*this, std::move(cmd->device_));
      }
      else
         // Время переинициализации еще не пришло, продолжаем IO-операции.
         send_perform_io_msg(std::move(cmd->device_));
   }

   void handle_msg_delay(
         a_dashboard_t::op_type_t op_type,
         const msg_base_t & msg ) const {
      const auto delta = clock_t::now() - msg.expected_time_;
      so_5::send<a_dashboard_t::delay_info_t>(
            dashboard_mbox_, op_type, delta);
   }

   std::chrono::milliseconds calculate_io_period() const {
      const long long min = args_.io_ops_period_.left_.count();
      const long long max = args_.io_ops_period_.right_.count();

      std::random_device rd_dev;
      std::uniform_int_distribution<long long> rd_seq{min, max};

      return std::chrono::milliseconds{rd_seq(rd_dev)};
   }

   unsigned calculate_io_ops_before_reinit() const {
      std::random_device rd_dev;
      std::uniform_int_distribution<unsigned> rd_seq{1, args_.io_ops_before_reinit_};

      return rd_seq(rd_dev);
   }

   unsigned calculate_reinits_before_recreate() const {
      std::random_device rd_dev;
      std::uniform_int_distribution<unsigned> rd_seq{1, args_.reinits_before_recreate_};

      return rd_seq(rd_dev);
   }

   void send_perform_io_msg(device_uptr_t dev) const {
      const auto period = dev->io_period_;
      const auto expected_time = clock_t::now() + period;
      so_5::send_delayed<so_5::mutable_msg<perform_io_t>>(
            *this, period, std::move(dev), expected_time);
   }

};

