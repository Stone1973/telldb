#include <tellstore/ClientConfig.hpp>

#include <crossbow/allocator.hpp>
#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/logger.hpp>
#include <crossbow/program_options.hpp>
#include <crossbow/string.hpp>

using namespace tell;
using namespace tell::store;

int main(int argc, const char** argv) {
    crossbow::string commitManagerHost;
    crossbow::string tellStoreHost;
    ClientConfig clientConfig;
    bool help = false;
    crossbow::string logLevel("INFO");

    auto opts = crossbow::program_options::create_options(argv[0],
            crossbow::program_options::value<'h'>("help", &help),
            crossbow::program_options::value<'l'>("log-level", &logLevel),
            crossbow::program_options::value<'c'>("commit-manager", &commitManagerHost),
            crossbow::program_options::value<'s'>("server", &tellStoreHost),
            crossbow::program_options::value<'m'>("memory", &clientConfig.scanMemory),
            crossbow::program_options::value<-1>("network-threads", &clientConfig.numNetworkThreads,
                    crossbow::program_options::tag::ignore_short<true>{}));

    try {
        crossbow::program_options::parse(opts, argc, argv);
    } catch (crossbow::program_options::argument_not_found e) {
        std::cerr << e.what() << std::endl << std::endl;
        crossbow::program_options::print_help(std::cout, opts);
        return 1;
    }

    if (help) {
        crossbow::program_options::print_help(std::cout, opts);
        return 0;
    }

    clientConfig.commitManager = crossbow::infinio::Endpoint(crossbow::infinio::Endpoint::ipv4(), commitManagerHost);
    clientConfig.tellStore = crossbow::infinio::Endpoint(crossbow::infinio::Endpoint::ipv4(), tellStoreHost);

    crossbow::infinio::InfinibandLimits infinibandLimits;
    infinibandLimits.receiveBufferCount = 128;
    infinibandLimits.sendBufferCount = 128;
    infinibandLimits.bufferLength = 32 * 1024;
    infinibandLimits.sendQueueLength = 128;

    crossbow::logger::logger->config.level = crossbow::logger::logLevelFromString(logLevel);

    LOG_INFO("Starting TellDB benchmark");
    LOG_INFO("--- Commit Manager: %1%", clientConfig.commitManager);
    LOG_INFO("--- TellStore: %1%", clientConfig.tellStore);
    LOG_INFO("--- Network Threads: %1%", clientConfig.numNetworkThreads);
    LOG_INFO("--- Scan Memory: %1%GB", double(clientConfig.scanMemory) / double(1024 * 1024 * 1024));

    // Initialize allocator
    crossbow::allocator::init();

    // Initialize network stack
    crossbow::infinio::InfinibandService service(infinibandLimits);
    service.run();

    LOG_INFO("Exiting TellDB benchmark");
    return 0;
}
