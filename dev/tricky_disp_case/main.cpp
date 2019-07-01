#include <common/args.hpp>
#include <common/args_parser.hpp>

#include <common/a_device_manager.hpp>

#include <fmt/ostream.h>

#include <stack>

// Класс диспетчера, который будет обслуживать события агента a_device_manager_t.
// Этот диспетчер может быть использован только в качестве приватного
// диспетчера.
//
class tricky_dispatcher_t
      : public so_5::disp_binder_t
		, public so_5::event_queue_t {

   // Тип контейнера для рабочих очередей.
   using thread_pool_t = std::vector<std::thread>;

   // Каналы, которые будут использоваться в качестве очередей сообщений.
   so_5::mchain_t init_reinit_ch_;
   so_5::mchain_t other_demands_ch_;

   // Рабочие очереди этого диспетчера.
   thread_pool_t work_threads_;

   static const std::type_index init_device_type;
   static const std::type_index reinit_device_type;
   
   // Вспомогательный метод для вычисления размеров подпулов.
   static auto calculate_pools_sizes(unsigned pool_size) {
      if( 2u == pool_size)
         // Всего две очереди в пуле. Делим пополам.
         return std::make_tuple(1u, 1u);
      else {
         // Нитей первого типа будет 3/4 от общего количества.
         const auto first_pool_size = (pool_size/4u)*3u;
         return std::make_tuple(first_pool_size, pool_size - first_pool_size);
      }
   }

   // Вспомогательный метод для того, чтобы завершить работу всех нитей.
   void shutdown_work_threads() noexcept {
      // Сначала закроем оба канала.
      so_5::close_drop_content(init_reinit_ch_);
      so_5::close_drop_content(other_demands_ch_);

      // Теперь можно дождаться момента, когда все рабочие нити закончат
      // свою работу.
      for(auto & t : work_threads_)
         t.join();

      // Пул рабочих нитей должен быть очищен.
      work_threads_.clear();
   }

   // Запуск всех рабочих нитей.
   // Если в процессе запуска произойдет сбой, то ранее запущенные нити
   // должны быть остановлены.
   void launch_work_threads(
         unsigned first_type_threads_count,
         unsigned second_type_threads_count) {
      work_threads_.reserve(first_type_threads_count + second_type_threads_count);
      try {
         for(auto i = 0u; i < first_type_threads_count; ++i)
            work_threads_.emplace_back([this]{ first_type_thread_body(); });

         for(auto i = 0u; i < second_type_threads_count; ++i)
            work_threads_.emplace_back([this]{ second_type_thread_body(); });
      }
      catch(...) {
         shutdown_work_threads();
         throw; // Пусть с исключениями разбираются выше.
      }
   }

   // Обработчик объектов so_5::execution_demand_t.
   static void exec_demand_handler(so_5::execution_demand_t d) {
      d.call_handler(so_5::null_current_thread_id());
   }

   // Тело рабочей нити первого типа.
   void first_type_thread_body() {
      // Выполняем работу до тех пор, пока не будут закрыты все каналы.
      so_5::select(so_5::from_all().handle_all(),
            case_(init_reinit_ch_, exec_demand_handler),
            case_(other_demands_ch_, exec_demand_handler));
   }

   // Тело рабочей нити второго типа.
   void second_type_thread_body() {
      // Выполняем работу до тех пор, пока не будут закрыты все каналы.
      so_5::select(so_5::from_all().handle_all(),
            case_(other_demands_ch_, exec_demand_handler));
   }

	virtual void preallocate_resources( so_5::agent_t & /*agent*/ ) override {
		// There is no need to do something.
	}

	virtual void undo_preallocation( so_5::agent_t & /*agent*/ ) noexcept override {
		// There is no need to do something.
	}

	virtual void bind( so_5::agent_t & agent ) noexcept override {
		agent.so_bind_to_dispatcher( *this );
	}

	virtual void unbind( so_5::agent_t & /*agent*/ ) noexcept override {
		// There is no need to do something.
	}

   // Сохранение очередной заявки в очередях заявок.
   void push(so_5::execution_demand_t demand) override {
      if(init_device_type == demand.m_msg_type ||
            reinit_device_type == demand.m_msg_type) {
         // Эти заявки должны идти в свою собственную очередь.
         so_5::send<so_5::execution_demand_t>(init_reinit_ch_, std::move(demand));
      }
      else {
         // Это заявка, которая должна попасть в общую очередь.
         so_5::send<so_5::execution_demand_t>(other_demands_ch_, std::move(demand));
      }
   }

public:
   // Конструктор сразу же запускает все рабочие нити.
   tricky_dispatcher_t(
         // SObjectizer Environment, на котором нужно будет работать.
         so_5::environment_t & env,
         // Количество рабочих потоков, которые должны быть созаны диспетчером.
         unsigned pool_size)
         :  init_reinit_ch_{so_5::create_mchain(env)}
         ,  other_demands_ch_{so_5::create_mchain(env)} {
      const auto [first_type_count, second_type_count] =
            calculate_pools_sizes(pool_size);

      launch_work_threads(first_type_count, second_type_count);
   }
   ~tricky_dispatcher_t() noexcept {
      // Все работающие нити должны быть остановлены.
      shutdown_work_threads();
   }

   // Метод-фабрика для создания экземпляров диспетчера.
   static so_5::disp_binder_shptr_t make(
			so_5::environment_t & env, unsigned pool_size) {
      return std::make_shared<tricky_dispatcher_t>(env, pool_size);
   }
};

const std::type_index tricky_dispatcher_t::init_device_type =
      typeid(a_device_manager_t::init_device_t);
const std::type_index tricky_dispatcher_t::reinit_device_type =
      typeid(so_5::mutable_msg<a_device_manager_t::reinit_device_t>);

void run_example(const args_t & args ) {
   print_args(args);

   so_5::launch([&](so_5::environment_t & env) {
         env.introduce_coop([&](so_5::coop_t & coop) {
            const auto dashboard_mbox =
                  coop.make_agent<a_dashboard_t>()->so_direct_mbox();

            // Агента для управления устройствами запускаем на отдельном
            // хитром диспетчере.
            coop.make_agent_with_binder<a_device_manager_t>(
                  tricky_dispatcher_t::make(env, args.thread_pool_size_),
                  args,
                  dashboard_mbox);
         });
      });
}

int main(int argc, char ** argv) {
   try {
      const auto r = parse_args(argc, argv);
      if(auto a = std::get_if<args_t>(&r))
         run_example(*a);

      return 0;
   }
   catch(const std::exception & x) {
      std::cerr << "Exception caught: " << x.what() << std::endl;
   }

   return 2;
}

