#include <common/args.hpp>
#include <common/args_parser.hpp>

#include <common/a_device_manager.hpp>

#include <fmt/ostream.h>

#include <stack>

// A class of dispatcher intended to process events of a_device_manager_t agent.
class tricky_dispatcher_t
      : public so_5::disp_binder_t
		, public so_5::event_queue_t {

   // Type of container for worker threads.
   using thread_pool_t = std::vector<std::thread>;

	// Channels to be used as event-queues.
   so_5::mchain_t init_reinit_ch_;
   so_5::mchain_t other_demands_ch_;

   // The pool of worker threads for that dispatcher.
   thread_pool_t work_threads_;

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
		// Both channels should be closed first.
      so_5::close_drop_content(init_reinit_ch_);
      so_5::close_drop_content(other_demands_ch_);

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
         for(auto i = 0u; i < first_type_threads_count; ++i)
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

   // The body for a thread of the first type.
   void first_type_thread_body() {
		// Run until all channels will be closed.
      so_5::select(so_5::from_all().handle_all(),
            case_(init_reinit_ch_, exec_demand_handler),
            case_(other_demands_ch_, exec_demand_handler));
   }

   // The body for a thread of the second type.
   void second_type_thread_body() {
		// Run until all channels will be closed.
      so_5::select(so_5::from_all().handle_all(),
            case_(other_demands_ch_, exec_demand_handler));
   }

	// Implementation of the methods inherited from disp_binder.
	void preallocate_resources(so_5::agent_t & /*agent*/) override {
		// There is no need to do something.
	}

	void undo_preallocation(so_5::agent_t & /*agent*/) noexcept override {
		// There is no need to do something.
	}

	void bind(so_5::agent_t & agent) noexcept override {
		agent.so_bind_to_dispatcher(*this);
	}

	void unbind(so_5::agent_t & /*agent*/) noexcept override {
		// There is no need to do something.
	}

   // Implementation of the method inherited from event_queue.
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

public:
	// The constructor that starts all worker threads.
   tricky_dispatcher_t(
			// SObjectizer Environment to work in.
         so_5::environment_t & env,
			// The size of the thread pool.
         unsigned pool_size)
         :  init_reinit_ch_{so_5::create_mchain(env)}
         ,  other_demands_ch_{so_5::create_mchain(env)} {
      const auto [first_type_count, second_type_count] =
            calculate_pools_sizes(pool_size);

      launch_work_threads(first_type_count, second_type_count);
   }
   ~tricky_dispatcher_t() noexcept {
      // All worker threads should be stopped.
      shutdown_work_threads();
   }

   // A factory for the creation of the dispatcher.
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

