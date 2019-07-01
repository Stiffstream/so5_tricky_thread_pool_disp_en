#include <common/args.hpp>
#include <common/args_parser.hpp>

#include <common/a_device_manager.hpp>

void run_example(const args_t & args ) {
   print_args(args);

   so_5::launch([&](so_5::environment_t & env) {
         env.introduce_coop([&](so_5::coop_t & coop) {
            const auto dashboard_mbox =
                  coop.make_agent<a_dashboard_t>()->so_direct_mbox();

				// Run the device manager on a separate adv_thread_pool-dispatcher.
            namespace disp = so_5::disp::adv_thread_pool;
            coop.make_agent_with_binder<a_device_manager_t>(
                  disp::make_dispatcher(env, args.thread_pool_size_).
                        binder(disp::bind_params_t{}),
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

