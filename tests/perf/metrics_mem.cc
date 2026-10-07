/*
 * Measures the memory held by the metrics metadata built for a Redpanda-shaped
 * set of per-partition series on one shard.
 */

#include <seastar/core/app-template.hh>
#include <seastar/core/memory.hh>
#include <seastar/core/metrics.hh>
#include <seastar/core/metrics_api.hh>
#include <seastar/core/metrics_registration.hh>
#include <seastar/core/thread.hh>
#include <fmt/core.h>

namespace bpo = boost::program_options;
namespace sm = seastar::metrics;
using namespace seastar;

int main(int ac, char** av) {
    app_template app;
    app.add_options()
        ("partitions", bpo::value<size_t>()->default_value(5000), "partition replicas on this shard")
        ("topics", bpo::value<size_t>()->default_value(50), "topics the partitions are spread over")
        ("topic-len", bpo::value<size_t>()->default_value(20), "topic name length")
        ("aggregate", bpo::value<bool>()->default_value(true), "aggregate_metrics on (internal families aggregate {shard, partition})");
    return app.run(ac, av, [&] {
        return async([&] {
            auto& cfg = app.configuration();
            const size_t partitions = cfg["partitions"].as<size_t>();
            const size_t topics = cfg["topics"].as<size_t>();
            const size_t topic_len = cfg["topic-len"].as<size_t>();
            const bool aggregate = cfg["aggregate"].as<bool>();

            sm::label ns("namespace"), topic("topic"), partition("partition");
            sm::label pns("redpanda_namespace"), ptopic("redpanda_topic"), ppartition("redpanda_partition"), preq("redpanda_request");

            // Per partition replica, matching redpanda dev: 67 internal series that aggregate
            // {shard, partition} only when aggregate_metrics is on, 2 internal series that never
            // aggregate, 6 public series that always aggregate {shard, redpanda_partition}
            // (3 of them in one family split by redpanda_request) and 2 public that never do.
            constexpr size_t internal_agg = 67, internal_plain = 2, public_plain = 2;
            const std::vector<sm::label> internal_agg_labels = aggregate
                ? std::vector<sm::label>{sm::shard_label, partition} : std::vector<sm::label>{};
            const std::vector<sm::label> public_agg_labels{sm::shard_label, ppartition};

            std::vector<std::unique_ptr<sm::metric_groups>> groups;
            auto before_reg = memory::stats().allocated_memory();
            for (size_t p = 0; p < partitions; ++p) {
                auto tname = fmt::format("{:t>{}}", p % topics, topic_len);
                std::vector<sm::label_instance> il{ns("kafka"), topic(tname), partition(p / topics)};
                std::vector<sm::label_instance> pl{pns("kafka"), ptopic(tname), ppartition(p / topics)};
                std::vector<sm::metric_definition> defs;
                for (size_t i = 0; i < internal_agg; ++i) {
                    defs.push_back(sm::make_counter(fmt::format("internal_agg_{}", i), [] { return 1; }, sm::description("d"), il)
                            .aggregate(internal_agg_labels));
                }
                for (size_t i = 0; i < internal_plain; ++i) {
                    defs.push_back(sm::make_gauge(fmt::format("internal_plain_{}", i), [] { return 1; }, sm::description("d"), il));
                }
                for (auto req : {"produce", "consume", "follower_consume"}) {
                    auto l = pl;
                    l.push_back(preq(req));
                    defs.push_back(sm::make_counter("public_request_bytes_total", [] { return 1; }, sm::description("d"), l)
                            .aggregate(public_agg_labels));
                }
                for (size_t i = 0; i < 3; ++i) {
                    defs.push_back(sm::make_counter(fmt::format("public_agg_{}", i), [] { return 1; }, sm::description("d"), pl)
                            .aggregate(public_agg_labels));
                }
                for (size_t i = 0; i < public_plain; ++i) {
                    defs.push_back(sm::make_gauge(fmt::format("public_plain_{}", i), [] { return 1; }, sm::description("d"), pl));
                }
                auto g = std::make_unique<sm::metric_groups>();
                g->add_group("rp", defs);
                groups.push_back(std::move(g));
            }
            const size_t per_partition = internal_agg + internal_plain + 6 + public_plain;
            const size_t series = partitions * per_partition;

            auto s0 = memory::stats();
            auto md = sm::impl::get_local_impl()->metadata();
            auto s1 = memory::stats();
            size_t meta_bytes = s1.allocated_memory() - s0.allocated_memory();
            fmt::print("partitions={} topics={} topic_len={} aggregate={} series={}\n",
                    partitions, topics, topic_len, aggregate, series);
            fmt::print("registration: {:.1f} MiB\n", (s0.allocated_memory() - before_reg) / 1048576.0);
            fmt::print("metadata: {} bytes = {:.2f} MiB, {:.1f} B/partition, {:.2f} B/series, net mallocs {}\n",
                    meta_bytes, meta_bytes / 1048576.0, double(meta_bytes) / partitions, double(meta_bytes) / series,
                    int64_t(s1.mallocs() - s0.mallocs()) - int64_t(s1.frees() - s0.frees()));
        });
    });
}
