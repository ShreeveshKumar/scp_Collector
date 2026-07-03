#include "sia/db.hpp"

#include <chrono>
#include <utility>

#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/types.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/options/update.hpp>
#include <mongocxx/uri.hpp>

namespace sia {
namespace {

// The mongocxx driver requires exactly one instance for the whole program,
// created before any other driver use and destroyed after all of it.
void ensure_instance() {
    static mongocxx::instance instance{};
    (void)instance;
}

bsoncxx::types::b_date now_date() {
    return bsoncxx::types::b_date{std::chrono::system_clock::now()};
}

}  // namespace

struct Db::Impl {
    mongocxx::client client;
    std::string database;
    std::string collection;

    Impl(const std::string& uri, std::string db, std::string coll)
        : client(mongocxx::uri{uri}),
          database(std::move(db)),
          collection(std::move(coll)) {}

    mongocxx::collection coll() { return client[database][collection]; }
};

Db::Db(std::string uri, std::string database, std::string collection) {
    ensure_instance();
    impl_ = std::make_unique<Impl>(uri, std::move(database), std::move(collection));
}

Db::~Db() = default;

void Db::ensure_schema() {
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;
    // Unique index on external_id makes upsert idempotent across repeated runs.
    impl_->coll().create_index(make_document(kvp("external_id", 1)),
                               make_document(kvp("unique", true)));
}

bool Db::upsert(const Startup& s) {
    using bsoncxx::builder::basic::array;
    using bsoncxx::builder::basic::kvp;
    using bsoncxx::builder::basic::make_document;

    array founders_arr;
    for (const auto& f : s.founders) founders_arr.append(f);
    array tech_arr;
    for (const auto& t : s.tech_stack) tech_arr.append(t);

    auto set_doc = make_document(
        kvp("source", s.source),
        kvp("name", s.name),
        kvp("url", s.url),
        kvp("description", s.description),
        kvp("founders", founders_arr.extract()),
        kvp("tech_stack", tech_arr.extract()),
        kvp("industry", s.industry),
        kvp("has_funding", s.has_funding),
        kvp("funding_note", s.funding_note),
        kvp("discovered_at", s.discovered_at),
        kvp("updated_at", now_date()));

    auto set_on_insert = make_document(
        kvp("external_id", s.external_id),
        kvp("created_at", now_date()));

    auto filter = make_document(kvp("external_id", s.external_id));
    auto update = make_document(kvp("$set", set_doc),
                               kvp("$setOnInsert", set_on_insert));

    mongocxx::options::update opts{};
    opts.upsert(true);

    auto result = impl_->coll().update_one(filter.view(), update.view(), opts);
    return result && result->upserted_count() > 0;
}

long Db::count() {
    using bsoncxx::builder::basic::make_document;
    return static_cast<long>(impl_->coll().count_documents(make_document()));
}

}  // namespace sia
