#include <common/args.hpp>
#include <common/args_parser.hpp>

#include <common/a_device_manager.hpp>

#include <fmt/ostream.h>

// A class of dispatcher intended to process events of a_device_manager_t agent.
class tricky_dispatcher_t final
      : public so_5::disp_binder_t
      , public so_5::event_queue_t {

   // A kind of std::latch from C++20, but without a fixed number of participant.
   // It's something similar to Run-Down Protection from Windows's kernel:
   //
   // https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/run-down-protection
   class rundown_latch_t {
      std::mutex lock_;
      std::condition_variable wakeup_cv_;

      bool closed_{false};
      unsigned attenders_{};

   public:
      rundown_latch_t() = default;

      void acquire() {
         std::lock_guard<std::mutex> lock{lock_};
         if(closed_)
            throw std::runtime_error{"rundown_latch is closed"};
         ++attenders_;
      }

      void release() noexcept {
         std::lock_guard<std::mutex> lock{lock_};
         --attenders_;
         if(!attenders_)
            wakeup_cv_.notify_all();
      }

      void wait_then_close() {
         std::unique_lock<std::mutex> lock{lock_};
         if(attenders_)
         {
            wakeup_cv_.wait(lock, [this]{ return 0u == attenders_; });
            closed_ = true;
         }
      }
   };

   // A kind of std::lock_guard, but for rundown_latch_t.
   class auto_acquire_release_rundown_latch_t {
      rundown_latch_t & room_;

   public:
      auto_acquire_release_rundown_latch_t(rundown_latch_t & room) : room_{room} {
         room_.acquire();
      }
      ~auto_acquire_release_rundown_latch_t() {
         room_.release();
      }
   };

   // Type of container for worker threads.
   using thread_pool_t = std::vector<std::thread>;

   // Channels to be used as event-queues.
   so_5::mchain_t start_finish_ch_;
   so_5::mchain_t init_reinit_ch_;
   so_5::mchain_t other_demands_ch_;

   // The pool of worker threads for that dispatcher.
   thread_pool_t work_threads_;

   // Synchronization objects required for thread management.
   //
   // This one is for starting worker threads.
   // The leader thread should wait while all workers are created.
   rundown_latch_t launch_room_;
   // This one is for handling evt_start,
   // All workers (except the leader) have to wait while evt_start completed.
   rundown_latch_t start_room_;
   // This on is for handling evt_finish.
   // The leader thread has to wait while all workers complete their work.
   rundown_latch_t finish_room_;

   static const std::type_index init_device_type;
   static const std::type_index reinit_device_type;

   // Helper method for calculation of sizes of sub-pools.
   static auto calculate_pools_sizes(unsigned pool_size) {
      if( 2u == pool_size)
         // Only two thread in the pool. Use one thread for each sub-pool.
         return std::make_tuple(1u, 1u);
      else {
         // Threads of the first type will be 3/4 of the total count of threads.
         const auto first_pool_size = (pool_size/4u)*3u;
         return std::make_tuple(first_pool_size, pool_size - first_pool_size);
      }
   }

   // Helper method for shutdown and join all threads.
   void shutdown_work_threads() noexcept {
      // All channels should be closed first.
      so_5::close_drop_content(so_5::terminate_if_throws, start_finish_ch_);
      so_5::close_drop_content(so_5::terminate_if_throws, init_reinit_ch_);
      so_5::close_drop_content(so_5::terminate_if_throws, other_demands_ch_);

      // Now all threads can be joined.
      for(auto & t : work_threads_)
         t.join();

      // The pool should be dropped.
      work_threads_.clear();
   }

   // Launch all threads.
   // If there is an error then all previously started threads
   // should be stopped.
   void launch_work_threads(
         unsigned first_type_threads_count,
         unsigned second_type_threads_count) {
      work_threads_.reserve(first_type_threads_count + second_type_threads_count);
      try {
         // The leader has to be suspended until all workers will be created.
         auto_acquire_release_rundown_latch_t launch_room_changer{launch_room_};

         // Start the leader thread first.
         work_threads_.emplace_back([this]{ leader_thread_body(); });

         // Now we can launch all remaining workers.
         for(auto i = 1u; i < first_type_threads_count; ++i)
            work_threads_.emplace_back([this]{ first_type_thread_body(); });

         for(auto i = 0u; i < second_type_threads_count; ++i)
            work_threads_.emplace_back([this]{ second_type_thread_body(); });
      }
      catch(...) {
         shutdown_work_threads();
         throw; // Rethrow an exception to be handled somewhere upper.
      }
   }

   // A handler for so_5::execution_demand_t.
   static void exec_demand_handler(so_5::execution_demand_t d) {
      d.call_handler(so_5::null_current_thread_id());
   }

   // The body of the leader thread.
   void leader_thread_body() {
      // We have to wait while all workers are created.
      // NOTE: not all of them can start their work actually, but all
      // std::thread objects should be created.
      launch_room_.wait_then_close();

      {
         // We have to block all other threads until evt_start will be processed.
         auto_acquire_release_rundown_latch_t start_room_changer{start_room_};
         // Process evt_start.
         so_5::receive(so_5::from(start_finish_ch_).handle_n(1),
               exec_demand_handler);
      }

      // Now the leader can play the role of the first thread type.
      first_type_thread_body();

      // All worker should finish their work before processing of evt_finish.
      finish_room_.wait_then_close();

      // Process evt_finish.
      so_5::receive(so_5::from(start_finish_ch_).handle_n(1),
            exec_demand_handler);
   }

   // The body for a thread of the first type.
   void first_type_thread_body() {
      // Processing of evt_finish has to be enabled at the end.
      auto_acquire_release_rundown_latch_t finish_room_changer{finish_room_};

      // Wait while evt_start is processed.
      start_room_.wait_then_close();

      // Run until all channels will be closed.
      so_5::select(so_5::from_all().handle_all(),
            receive_case(init_reinit_ch_, exec_demand_handler),
            receive_case(other_demands_ch_, exec_demand_handler));
   }

   // The body for a thread of the second type.
   void second_type_thread_body() {
      // Processing of evt_finish has to be enabled at the end.
      auto_acquire_release_rundown_latch_t finish_room_changer{finish_room_};

      // Wait while evt_start is processed.
      start_room_.wait_then_close();

      // Run until all channels will be closed.
      so_5::select(so_5::from_all().handle_all(),
            receive_case(other_demands_ch_, exec_demand_handler));
   }

   // Implementation of the methods inherited from disp_binder.
   void preallocate_resources(so_5::agent_t & /*agent*/) override {
      // Nothing to do.
   }

   void undo_preallocation(so_5::agent_t & /*agent*/) noexcept override {
      // Nothing to do.
   }

   void bind(so_5::agent_t & agent) noexcept override {
      agent.so_bind_to_dispatcher(*this);
   }

   void unbind(so_5::agent_t & agent) noexcept override {
      // Nothing to do.
   }

   // Implementation of the methods inherited from event_queue.
   void push(so_5::execution_demand_t demand) override {
      if(init_device_type == demand.m_msg_type ||
            reinit_device_type == demand.m_msg_type) {
         // That demand should go to a separate queue.
         so_5::send<so_5::execution_demand_t>(init_reinit_ch_, std::move(demand));
      }
      else {
         // That demand should go to the common queue.
         so_5::send<so_5::execution_demand_t>(other_demands_ch_, std::move(demand));
      }
   }

   void push_evt_start(so_5::execution_demand_t demand) override {
      so_5::send<so_5::execution_demand_t>(start_finish_ch_, std::move(demand));
   }

   // NOTE: don't care about exception, if the demand can't be stored
   // into the queue the application has to be aborted anyway.
   void push_evt_finish(so_5::execution_demand_t demand) noexcept override {
      // Chains for "ordinary" messages has to be closed.
      so_5::close_retain_content(so_5::terminate_if_throws, init_reinit_ch_);
      so_5::close_retain_content(so_5::terminate_if_throws, other_demands_ch_);

      // Now we can store the evt_finish demand in the special chain.
      so_5::send<so_5::execution_demand_t>(start_finish_ch_, std::move(demand));
   }

public:
   // The constructor that starts all worker threads.
   tricky_dispatcher_t(
         // SObjectizer Environment to work in.
         so_5::environment_t & env,
         // The size of the thread pool.
         unsigned pool_size)
         :  start_finish_ch_{
               so_5::create_mchain(env,
                     2u, // Just evt_start and evt_finish.
                     so_5::mchain_props::memory_usage_t::preallocated,
                     so_5::mchain_props::overflow_reaction_t::abort_app)
            }
         ,  init_reinit_ch_{so_5::create_mchain(env)}
         ,  other_demands_ch_{so_5::create_mchain(env)}
   {
      const auto [first_type_count, second_type_count] =
            calculate_pools_sizes(pool_size);

      launch_work_threads(first_type_count, second_type_count);
   }
   ~tricky_dispatcher_t() noexcept {
      // All worker threads should be stopped.
      shutdown_work_threads();
   }

   // A factory for the creation of the dispatcher.
   [[nodiscard]]
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

            // Run the device manager of an instance of our tricky dispatcher.
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

