#pragma once

#include <common/args.hpp>
#include <common/a_dashboard.hpp>

#include <random>

class a_device_manager_t final : public so_5::agent_t {
public:
   using clock_t = a_dashboard_t::clock_t;

   struct msg_base_t : public so_5::message_t {
		// Time of expected arrival of a message.
      clock_t::time_point expected_time_;

		// The default constructor.
		// Uses the current time as expected_time_'s value.
      msg_base_t()
         :  expected_time_(clock_t::now())
      {}
		// Initializing constructor.
		// A value for expected_time_ is specified explicitelly.
      msg_base_t(clock_t::time_point expected_time)
         :  expected_time_(expected_time)
      {}
   };

	// A description of one device.
	// An object of that type is created at the start and then is resent
	// inside device-related messages.
   struct device_t final {
      using id_t = std::uint_fast64_t;
		// The unique ID of a device.
      id_t id_;
		// A time between IO-ops for that device.
		// Recalculated at reinits of the device.
      std::chrono::milliseconds io_period_;
		// The count of remaining IO-ops for that device.
		// The reinit is initialized when remaining_io_ops_ becomes zero.
      unsigned remaining_io_ops_;
		// The count of remaining reinits for that device.
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

	// A message about necessity of initialization of a new device.
   struct init_device_t final : public msg_base_t {
      device_t::id_t id_;

      init_device_t(device_t::id_t id) : id_(id) {}
   };

	// A message about necessity of reinitialization of a device.
   struct reinit_device_t final : public msg_base_t {
      device_uptr_t device_;

      reinit_device_t(device_uptr_t device) : device_(std::move(device)) {}
   };

	// A message about necessity to perform an IO-op on a device.
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

   void so_evt_start() override {
		// Send a bunch of messages for the creation of new devices.
      device_t::id_t id{};
      for(unsigned i = 0; i != args_.device_count_; ++i, ++id)
         so_5::send<init_device_t>(*this, id);
   }

private:
   const args_t args_;
   const so_5::mbox_t dashboard_mbox_;

   void on_init_device(mhood_t<init_device_t> cmd) const {
		// Update the stats for that op.
      handle_msg_delay(a_dashboard_t::op_type_t::init, *cmd);

		// A new device should be created.
		// We should imitate a pause related to the device initialization.
      auto dev = std::make_unique<device_t>(cmd->id_,
            calculate_io_period(),
            calculate_io_ops_before_reinit(),
            calculate_reinits_before_recreate());

      std::this_thread::sleep_for(args_.device_init_time_);

		// Send a message for the first IO-op on that device.
      send_perform_io_msg(std::move(dev));
   }

   void on_reinit_device(mutable_mhood_t<reinit_device_t> cmd) const {
		// Update the stats for that op.
      handle_msg_delay(a_dashboard_t::op_type_t::reinit, *cmd);

		// The main params of the device should be updated.
      cmd->device_->io_period_ = calculate_io_period();
      cmd->device_->remaining_io_ops_ = calculate_io_ops_before_reinit();
      cmd->device_->remaining_reinits_ -= 1;

		// Simulate a pause of reinitializing the device.
		// Reinitialization takes 2/3 from init's time.
      std::this_thread::sleep_for((args_.device_init_time_/3)*2);

		// Continue to do IO-op on that device.
      send_perform_io_msg(std::move(cmd->device_));
   }

   void on_perform_io(mutable_mhood_t<perform_io_t> cmd) const {
		// Update the stats for that op.
      handle_msg_delay(a_dashboard_t::op_type_t::io_op, *cmd);

		// Simulate a pause for IO-op.
      std::this_thread::sleep_for(args_.io_op_time_);

		// The remaining count of IO-ops should be decremented.
      cmd->device_->remaining_io_ops_ -= 1;
		// Maybe it is time to reinit or recreate the device?
      if(0 == cmd->device_->remaining_io_ops_) {
         if(0 == cmd->device_->remaining_reinits_)
				// The device should recreated. Using the same ID.
            so_5::send<init_device_t>(*this, cmd->device_->id_);
         else
				// There are remaining reinit attempts.
            so_5::send<so_5::mutable_msg<reinit_device_t>>(*this, std::move(cmd->device_));
      }
      else
			// It isn't time for reinit yet. Continue IO-operations.
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

