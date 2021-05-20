#include "realm/object-store/shared_realm.hpp"
#include "realm/object-store/object_store.hpp"
#include "realm/object-store/object_schema.hpp"
#include "realm/object-store/thread_safe_reference.hpp"
#include "type_traits"
#include "future"
#include "functional"
#include "queue"
#include "unordered_map"
#include "napi.h"
#include "realm/object-store/util/scheduler.hpp"
namespace realm::js::node {
using RealmConfig = Realm::Config;
using Scheduler = util::Scheduler;

namespace {
auto scheduler_queue = std::queue<std::function<void()>>();
auto scheduler = [] {
    auto sched = util::Scheduler::make_default();
    sched->set_notify_callback([] {
        while (!scheduler_queue.empty()) {
            scheduler_queue.front()();
            scheduler_queue.pop();
        }
    });
    return sched;
}();

void run_async_on_main_thread(std::function<void()> f)
{
    scheduler_queue.push(std::move(f));
    scheduler->notify();
}

template <typename F, typename = std::enable_if_t<std::is_invocable_r_v<void, F>>>
void run_maybe_async_on_main_thread(F&& f)
{
    if (scheduler->is_on_thread()) {
        return std::forward<F>(f)();
    }
    run_async_on_main_thread(std::forward<F>(f));
}

template <typename F, typename Ret = decltype(std::declval<F>()())>
Ret run_blocking_on_main_thread(F&& f)
{
    if (scheduler->is_on_thread()) {
        return std::forward<F>(f)();
    }

    auto task = std::make_shared<std::packaged_task<Ret()>>(std::forward<F>(f));
    auto fut = task->get_future();
    run_async_on_main_thread([task = std::move(task)] {
        (*task)();
    });
    return fut.get();
}
} // namespace

class Node_Transaction;
class Node_ObjectStore;
class Node_Realm;
class Node_Scheduler;
class Node_Impl_Scheduler;
void makeNodeEnum_SchemaMode(Napi::Env env, Napi::Object exports);
SchemaMode nodeTo_SchemaMode(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_SchemaMode(Napi::Env env, SchemaMode val);
void makeNodeEnum_PropertyType(Napi::Env env, Napi::Object exports);
PropertyType nodeTo_PropertyType(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_PropertyType(Napi::Env env, PropertyType val);
Property nodeTo_Property(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_Property(Napi::Env env, const Property& obj);
VersionID nodeTo_VersionID(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_VersionID(Napi::Env env, const VersionID& obj);
ColKey nodeTo_ColKey(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_ColKey(Napi::Env env, const ColKey& obj);
TableKey nodeTo_TableKey(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_TableKey(Napi::Env env, const TableKey& obj);
ObjectSchema nodeTo_ObjectSchema(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_ObjectSchema(Napi::Env env, const ObjectSchema& obj);
RealmConfig nodeTo_RealmConfig(Napi::Env env, Napi::Value val);
Napi::Value nodeFrom_RealmConfig(Napi::Env env, const RealmConfig& obj);
const std::shared_ptr<Transaction>& NODE_TO_SHARED_Transaction(Napi::Value val);
Napi::Value NODE_FROM_SHARED_Transaction(Napi::Env env, std::shared_ptr<Transaction> ptr);
const ObjectStore& NODE_TO_CLASS_ObjectStore(Napi::Value val);
Napi::Value NODE_FROM_CLASS_ObjectStore(Napi::Env env, ObjectStore val);
const std::shared_ptr<Realm>& NODE_TO_SHARED_Realm(Napi::Value val);
Napi::Value NODE_FROM_SHARED_Realm(Napi::Env env, std::shared_ptr<Realm> ptr);
std::shared_ptr<Scheduler> NODE_TO_INTERFACE_Scheduler(Napi::Value val);
const std::shared_ptr<Scheduler>& NODE_TO_SHARED_Scheduler(Napi::Value val);
Napi::Value NODE_FROM_SHARED_Scheduler(Napi::Env env, std::shared_ptr<Scheduler> ptr);
Napi::Object moduleInit(Napi::Env env, Napi::Object exports);
const std::unordered_map<std::string_view, int> strToEnum_SchemaMode = {
    {"Automatic", 0}, {"Immutable", 1},          {"ReadOnlyAlternative", 2},
    {"ResetFile", 3}, {"AdditiveDiscovered", 4}, {"AdditiveExplicit", 5},
    {"Manual", 6}};
const std::unordered_map<int, std::string_view> enumToStr_SchemaMode = {
    {0, "Automatic"}, {1, "Immutable"},          {2, "ReadOnlyAlternative"},
    {3, "ResetFile"}, {4, "AdditiveDiscovered"}, {5, "AdditiveExplicit"},
    {6, "Manual"}};
const std::unordered_map<std::string_view, int> strToEnum_PropertyType = {
    {"Int", 0},    {"Bool", 1},     {"String", 2},         {"Data", 3},    {"Date", 4},      {"Float", 5},
    {"Double", 6}, {"Object", 7},   {"LinkingObjects", 8}, {"Mixed", 9},   {"ObjectId", 10}, {"Decimal", 11},
    {"UUID", 12},  {"Required", 0}, {"Nullable", 64},      {"Array", 128}, {"Set", 256},     {"Dictionary", 512}};
const std::unordered_map<int, std::string_view> enumToStr_PropertyType = {
    {0, "Int"},    {1, "Bool"},     {2, "String"},         {3, "Data"},    {4, "Date"},      {5, "Float"},
    {6, "Double"}, {7, "Object"},   {8, "LinkingObjects"}, {9, "Mixed"},   {10, "ObjectId"}, {11, "Decimal"},
    {12, "UUID"},  {0, "Required"}, {64, "Nullable"},      {128, "Array"}, {256, "Set"},     {512, "Dictionary"}};

class Node_Transaction : public Napi::ObjectWrap<Node_Transaction> {
public:
    Node_Transaction(const Napi::CallbackInfo& info);
    static Napi::Function init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference ctor;
    std::shared_ptr<Transaction> m_ptr;
};

class Node_ObjectStore : public Napi::ObjectWrap<Node_ObjectStore> {
public:
    Node_ObjectStore(const Napi::CallbackInfo& info);
    static Napi::Value meth_get_schema_version(const Napi::CallbackInfo& info);
    static Napi::Value meth_set_schema_version(const Napi::CallbackInfo& info);
    static Napi::Value meth_schema_from_group(const Napi::CallbackInfo& info);
    static Napi::Function init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference ctor;
    ObjectStore m_val;
};

class Node_Realm : public Napi::ObjectWrap<Node_Realm> {
public:
    Node_Realm(const Napi::CallbackInfo& info);
    Napi::Value meth_begin_transaction(const Napi::CallbackInfo& info);
    Napi::Value meth_commit_transaction(const Napi::CallbackInfo& info);
    Napi::Value meth_cancel_transaction(const Napi::CallbackInfo& info);
    Napi::Value meth_freeze(const Napi::CallbackInfo& info);
    Napi::Value meth_last_seen_transaction_version(const Napi::CallbackInfo& info);
    Napi::Value meth_read_group(const Napi::CallbackInfo& info);
    Napi::Value meth_duplicate(const Napi::CallbackInfo& info);
    Napi::Value meth_enable_wait_for_change(const Napi::CallbackInfo& info);
    Napi::Value meth_wait_for_change(const Napi::CallbackInfo& info);
    Napi::Value meth_wait_for_change_release(const Napi::CallbackInfo& info);
    Napi::Value meth_refresh(const Napi::CallbackInfo& info);
    Napi::Value meth_set_auto_refresh(const Napi::CallbackInfo& info);
    Napi::Value meth_notify(const Napi::CallbackInfo& info);
    Napi::Value meth_invalidate(const Napi::CallbackInfo& info);
    Napi::Value meth_compact(const Napi::CallbackInfo& info);
    Napi::Value meth_write_copy(const Napi::CallbackInfo& info);
    Napi::Value meth_write_copy_to(const Napi::CallbackInfo& info);
    Napi::Value meth_verify_thread(const Napi::CallbackInfo& info);
    Napi::Value meth_verify_in_write(const Napi::CallbackInfo& info);
    Napi::Value meth_verify_open(const Napi::CallbackInfo& info);
    Napi::Value meth_verify_notifications_available(const Napi::CallbackInfo& info);
    Napi::Value meth_verify_notifications_available_maybe_throw(const Napi::CallbackInfo& info);
    Napi::Value meth_close(const Napi::CallbackInfo& info);
    Napi::Value meth_file_format_upgraded_from_version(const Napi::CallbackInfo& info);
    static Napi::Value meth_get_shared_realm(const Napi::CallbackInfo& info);
    Napi::Value prop_config(const Napi::CallbackInfo& info);
    Napi::Value prop_schema(const Napi::CallbackInfo& info);
    Napi::Value prop_schema_version(const Napi::CallbackInfo& info);
    Napi::Value prop_is_in_transaction(const Napi::CallbackInfo& info);
    Napi::Value prop_is_frozen(const Napi::CallbackInfo& info);
    Napi::Value prop_is_in_migration(const Napi::CallbackInfo& info);
    Napi::Value prop_get_number_of_versions(const Napi::CallbackInfo& info);
    Napi::Value prop_read_transaction_version(const Napi::CallbackInfo& info);
    Napi::Value prop_current_transaction_version(const Napi::CallbackInfo& info);
    Napi::Value prop_auto_refresh(const Napi::CallbackInfo& info);
    Napi::Value prop_can_deliver_notifications(const Napi::CallbackInfo& info);
    Napi::Value prop_scheduler(const Napi::CallbackInfo& info);
    Napi::Value prop_is_closed(const Napi::CallbackInfo& info);
    Napi::Value prop_audit_context(const Napi::CallbackInfo& info);
    static Napi::Function init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference ctor;
    std::shared_ptr<Realm> m_ptr;
};

class Node_Scheduler : public Napi::ObjectWrap<Node_Scheduler> {
public:
    Node_Scheduler(const Napi::CallbackInfo& info);
    Napi::Value meth_notify(const Napi::CallbackInfo& info);
    Napi::Value meth_is_on_thread(const Napi::CallbackInfo& info);
    Napi::Value meth_is_same_as(const Napi::CallbackInfo& info);
    Napi::Value meth_can_deliver_notifications(const Napi::CallbackInfo& info);
    Napi::Value meth_set_notify_callback(const Napi::CallbackInfo& info);
    static Napi::Value meth_get_frozen(const Napi::CallbackInfo& info);
    static Napi::Value meth_make_default(const Napi::CallbackInfo& info);
    static Napi::Value meth_set_default_factory(const Napi::CallbackInfo& info);
    static Napi::Function init(Napi::Env env, Napi::Object exports);
    static Napi::FunctionReference ctor;
    std::shared_ptr<Scheduler> m_ptr;
};

class Node_Impl_Scheduler : public Scheduler {
public:
    Node_Impl_Scheduler(Napi::Value val);
    void notify() override;
    bool is_on_thread() const noexcept override;
    bool is_same_as(std::shared_ptr<Scheduler> const& other) const noexcept override;
    bool can_deliver_notifications() const noexcept override;
    void set_notify_callback(std::function<void()> callback) noexcept override;
    Napi::ObjectReference m_obj;
};
Node_Transaction::Node_Transaction(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Node_Transaction>(info)
{
    auto env = info.Env();
    if (info.Length() != 1 || !info[0].IsExternal())
        throw Napi::TypeError::New(env, "need 1 external argument");
    m_ptr = std::move(*info[0].As<Napi::External<std::shared_ptr<Transaction>>>().Data());
}
Napi::Function Node_Transaction::init(Napi::Env env, Napi::Object exports)
{
    auto func = DefineClass(env, "Transaction", {});
    ctor = Napi::Persistent(func);
    ctor.SuppressDestruct();
    exports.Set("Transaction", func);
    return func;
}
Node_ObjectStore::Node_ObjectStore(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Node_ObjectStore>(info)
{
    auto env = info.Env();
    if (info.Length() != 1 || !info[0].IsExternal())
        throw Napi::TypeError::New(env, "need 1 external argument");
    m_val = std::move(*info[0].As<Napi::External<ObjectStore>>().Data());
}
Napi::Value Node_ObjectStore::meth_get_schema_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    auto&& val = ObjectStore::get_schema_version(*(info[0].As<Napi::External<Group>>().Data()));
    return Napi::Number::New(env, val);
}
Napi::Value Node_ObjectStore::meth_set_schema_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 2)
        throw Napi::TypeError::New(env, "expected 2 arguments");

    ObjectStore::set_schema_version(*(info[0].As<Napi::External<Group>>().Data()),
                                    uint64_t(info[1].ToNumber().Int64Value()));
    return env.Undefined();
}
Napi::Value Node_ObjectStore::meth_schema_from_group(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    auto&& val = ObjectStore::schema_from_group(*(info[0].As<Napi::External<Group>>().Data()));
    return ([&] {
        auto&& arr = val;
        const size_t len = arr.size();
        auto out = Napi::Array::New(env, len);
        for (size_t i = 0; i < len; i++) {
            out[i] = nodeFrom_ObjectSchema(env, arr[i]);
        }
        return out;
    }());
}
Napi::Function Node_ObjectStore::init(Napi::Env env, Napi::Object exports)
{
    auto func = DefineClass(env, "ObjectStore",
                            {StaticMethod<&Node_ObjectStore::meth_get_schema_version>("get_schema_version"),
                             StaticMethod<&Node_ObjectStore::meth_set_schema_version>("set_schema_version"),
                             StaticMethod<&Node_ObjectStore::meth_schema_from_group>("schema_from_group")});
    ctor = Napi::Persistent(func);
    ctor.SuppressDestruct();
    exports.Set("ObjectStore", func);
    return func;
}
Node_Realm::Node_Realm(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Node_Realm>(info)
{
    auto env = info.Env();
    if (info.Length() != 1 || !info[0].IsExternal())
        throw Napi::TypeError::New(env, "need 1 external argument");
    m_ptr = std::move(*info[0].As<Napi::External<std::shared_ptr<Realm>>>().Data());
}
Napi::Value Node_Realm::meth_begin_transaction(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).begin_transaction();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_commit_transaction(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).commit_transaction();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_cancel_transaction(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).cancel_transaction();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_freeze(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).freeze();
    return NODE_FROM_SHARED_Realm(env, val);
}
Napi::Value Node_Realm::meth_last_seen_transaction_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).last_seen_transaction_version();
    return Napi::Number::New(env, val);
}
Napi::Value Node_Realm::meth_read_group(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).read_group();
    return Napi::External<Group>::New(env, &val);
}
Napi::Value Node_Realm::meth_duplicate(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).duplicate();
    return NODE_FROM_SHARED_Transaction(env, val);
}
Napi::Value Node_Realm::meth_enable_wait_for_change(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).enable_wait_for_change();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_wait_for_change(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).wait_for_change();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::meth_wait_for_change_release(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).wait_for_change_release();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_refresh(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).refresh();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::meth_set_auto_refresh(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    (*m_ptr).set_auto_refresh(info[0].ToBoolean().Value());
    return env.Undefined();
}
Napi::Value Node_Realm::meth_notify(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).notify();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_invalidate(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).invalidate();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_compact(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).compact();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::meth_write_copy(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).write_copy();
    return ([&] {
        auto arr = Napi::ArrayBuffer::New(env, val.get().size());
        memcpy(arr.Data(), val.get().data(), val.get().size());
        return arr;
    }());
}
Napi::Value Node_Realm::meth_write_copy_to(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 2)
        throw Napi::TypeError::New(env, "expected 2 arguments");

    (*m_ptr).write_copy(info[0].ToString().Utf8Value(), ([&] {
                            if (!info[1].IsArrayBuffer())
                                throw Napi::TypeError::New(env, "expected ArrayBuffer");
                            auto buf = info[1].As<Napi::ArrayBuffer>();
                            return BinaryData(static_cast<const char*>(buf.Data()), buf.ByteLength());
                        }()));
    return env.Undefined();
}
Napi::Value Node_Realm::meth_verify_thread(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).verify_thread();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_verify_in_write(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).verify_in_write();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_verify_open(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).verify_open();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_verify_notifications_available(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).verify_notifications_available();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_verify_notifications_available_maybe_throw(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    (*m_ptr).verify_notifications_available(info[0].ToBoolean().Value());
    return env.Undefined();
}
Napi::Value Node_Realm::meth_close(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).close();
    return env.Undefined();
}
Napi::Value Node_Realm::meth_file_format_upgraded_from_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).file_format_upgraded_from_version();
    return (!(val) ? env.Null() : (Napi::Number::New(env, (*val))));
}
Napi::Value Node_Realm::meth_get_shared_realm(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    auto&& val = Realm::get_shared_realm(nodeTo_RealmConfig(env, info[0]));
    return NODE_FROM_SHARED_Realm(env, val);
}
Napi::Value Node_Realm::prop_config(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).config();
    return nodeFrom_RealmConfig(env, val);
}
Napi::Value Node_Realm::prop_schema(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).schema();
    return ([&] {
        auto&& arr = val;
        const size_t len = arr.size();
        auto out = Napi::Array::New(env, len);
        for (size_t i = 0; i < len; i++) {
            out[i] = nodeFrom_ObjectSchema(env, arr[i]);
        }
        return out;
    }());
}
Napi::Value Node_Realm::prop_schema_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).schema_version();
    return Napi::Number::New(env, val);
}
Napi::Value Node_Realm::prop_is_in_transaction(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).is_in_transaction();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_is_frozen(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).is_frozen();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_is_in_migration(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).is_in_migration();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_get_number_of_versions(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).get_number_of_versions();
    return Napi::Number::New(env, val);
}
Napi::Value Node_Realm::prop_read_transaction_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).read_transaction_version();
    return nodeFrom_VersionID(env, val);
}
Napi::Value Node_Realm::prop_current_transaction_version(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).current_transaction_version();
    return (!(val) ? env.Null() : (nodeFrom_VersionID(env, (*val))));
}
Napi::Value Node_Realm::prop_auto_refresh(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).auto_refresh();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_can_deliver_notifications(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).can_deliver_notifications();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_scheduler(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).scheduler();
    return NODE_FROM_SHARED_Scheduler(env, val);
}
Napi::Value Node_Realm::prop_is_closed(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).is_closed();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Realm::prop_audit_context(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    auto&& val = (*m_ptr).audit_context();
    return Napi::External<AuditInterface>::New(env, &*val);
}
Napi::Function Node_Realm::init(Napi::Env env, Napi::Object exports)
{
    auto func = DefineClass(
        env, "Realm",
        {InstanceMethod<&Node_Realm::meth_begin_transaction>("begin_transaction"),
         InstanceMethod<&Node_Realm::meth_commit_transaction>("commit_transaction"),
         InstanceMethod<&Node_Realm::meth_cancel_transaction>("cancel_transaction"),
         InstanceMethod<&Node_Realm::meth_freeze>("freeze"),
         InstanceMethod<&Node_Realm::meth_last_seen_transaction_version>("last_seen_transaction_version"),
         InstanceMethod<&Node_Realm::meth_read_group>("read_group"),
         InstanceMethod<&Node_Realm::meth_duplicate>("duplicate"),
         InstanceMethod<&Node_Realm::meth_enable_wait_for_change>("enable_wait_for_change"),
         InstanceMethod<&Node_Realm::meth_wait_for_change>("wait_for_change"),
         InstanceMethod<&Node_Realm::meth_wait_for_change_release>("wait_for_change_release"),
         InstanceMethod<&Node_Realm::meth_refresh>("refresh"),
         InstanceMethod<&Node_Realm::meth_set_auto_refresh>("set_auto_refresh"),
         InstanceMethod<&Node_Realm::meth_notify>("notify"),
         InstanceMethod<&Node_Realm::meth_invalidate>("invalidate"),
         InstanceMethod<&Node_Realm::meth_compact>("compact"),
         InstanceMethod<&Node_Realm::meth_write_copy>("write_copy"),
         InstanceMethod<&Node_Realm::meth_write_copy_to>("write_copy_to"),
         InstanceMethod<&Node_Realm::meth_verify_thread>("verify_thread"),
         InstanceMethod<&Node_Realm::meth_verify_in_write>("verify_in_write"),
         InstanceMethod<&Node_Realm::meth_verify_open>("verify_open"),
         InstanceMethod<&Node_Realm::meth_verify_notifications_available>("verify_notifications_available"),
         InstanceMethod<&Node_Realm::meth_verify_notifications_available_maybe_throw>(
             "verify_notifications_available_maybe_throw"),
         InstanceMethod<&Node_Realm::meth_close>("close"),
         InstanceMethod<&Node_Realm::meth_file_format_upgraded_from_version>("file_format_upgraded_from_version"),
         StaticMethod<&Node_Realm::meth_get_shared_realm>("get_shared_realm"),
         InstanceAccessor<&Node_Realm::prop_config>("config"),
         InstanceAccessor<&Node_Realm::prop_schema>("schema"),
         InstanceAccessor<&Node_Realm::prop_schema_version>("schema_version"),
         InstanceAccessor<&Node_Realm::prop_is_in_transaction>("is_in_transaction"),
         InstanceAccessor<&Node_Realm::prop_is_frozen>("is_frozen"),
         InstanceAccessor<&Node_Realm::prop_is_in_migration>("is_in_migration"),
         InstanceAccessor<&Node_Realm::prop_get_number_of_versions>("get_number_of_versions"),
         InstanceAccessor<&Node_Realm::prop_read_transaction_version>("read_transaction_version"),
         InstanceAccessor<&Node_Realm::prop_current_transaction_version>("current_transaction_version"),
         InstanceAccessor<&Node_Realm::prop_auto_refresh>("auto_refresh"),
         InstanceAccessor<&Node_Realm::prop_can_deliver_notifications>("can_deliver_notifications"),
         InstanceAccessor<&Node_Realm::prop_scheduler>("scheduler"),
         InstanceAccessor<&Node_Realm::prop_is_closed>("is_closed"),
         InstanceAccessor<&Node_Realm::prop_audit_context>("audit_context")});
    ctor = Napi::Persistent(func);
    ctor.SuppressDestruct();
    exports.Set("Realm", func);
    return func;
}
Node_Scheduler::Node_Scheduler(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<Node_Scheduler>(info)
{
    auto env = info.Env();
    if (info.Length() != 1 || !info[0].IsExternal())
        throw Napi::TypeError::New(env, "need 1 external argument");
    m_ptr = std::move(*info[0].As<Napi::External<std::shared_ptr<Scheduler>>>().Data());
}
Napi::Value Node_Scheduler::meth_notify(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    (*m_ptr).notify();
    return env.Undefined();
}
Napi::Value Node_Scheduler::meth_is_on_thread(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).is_on_thread();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Scheduler::meth_is_same_as(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    auto&& val = (*m_ptr).is_same_as(NODE_TO_INTERFACE_Scheduler(info[0]));
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Scheduler::meth_can_deliver_notifications(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = (*m_ptr).can_deliver_notifications();
    return Napi::Boolean::New(env, val);
}
Napi::Value Node_Scheduler::meth_set_notify_callback(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    (*m_ptr).set_notify_callback(([&] {
        if (!info[0].IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        return [func = std::make_shared<Napi::FunctionReference>(Napi::Persistent(info[0].As<Napi::Function>()))]() {
            auto env = func->Env();
            return run_blocking_on_main_thread([&]() -> void {
                auto ret = func->MakeCallback(env.Undefined(), {

                                                               });
                return ((void)ret);
            });
        };
    }()));
    return env.Undefined();
}
Napi::Value Node_Scheduler::meth_get_frozen(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    auto&& val = Scheduler::get_frozen(nodeTo_VersionID(env, info[0]));
    return NODE_FROM_SHARED_Scheduler(env, val);
}
Napi::Value Node_Scheduler::meth_make_default(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 0)
        throw Napi::TypeError::New(env, "expected 0 arguments");

    auto&& val = Scheduler::make_default();
    return NODE_FROM_SHARED_Scheduler(env, val);
}
Napi::Value Node_Scheduler::meth_set_default_factory(const Napi::CallbackInfo& info)
{
    auto env = info.Env();
    if (info.Length() != 1)
        throw Napi::TypeError::New(env, "expected 1 arguments");

    Scheduler::set_default_factory(([&] {
        if (!info[0].IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        return [func = std::make_shared<Napi::FunctionReference>(Napi::Persistent(info[0].As<Napi::Function>()))]() {
            auto env = func->Env();
            return run_blocking_on_main_thread([&]() -> std::shared_ptr<Scheduler> {
                auto ret = func->MakeCallback(env.Undefined(), {

                                                               });
                return NODE_TO_INTERFACE_Scheduler(ret);
            });
        };
    }()));
    return env.Undefined();
}
Napi::Function Node_Scheduler::init(Napi::Env env, Napi::Object exports)
{
    auto func =
        DefineClass(env, "Scheduler",
                    {InstanceMethod<&Node_Scheduler::meth_notify>("notify"),
                     InstanceMethod<&Node_Scheduler::meth_is_on_thread>("is_on_thread"),
                     InstanceMethod<&Node_Scheduler::meth_is_same_as>("is_same_as"),
                     InstanceMethod<&Node_Scheduler::meth_can_deliver_notifications>("can_deliver_notifications"),
                     InstanceMethod<&Node_Scheduler::meth_set_notify_callback>("set_notify_callback"),
                     StaticMethod<&Node_Scheduler::meth_get_frozen>("get_frozen"),
                     StaticMethod<&Node_Scheduler::meth_make_default>("make_default"),
                     StaticMethod<&Node_Scheduler::meth_set_default_factory>("set_default_factory")});
    ctor = Napi::Persistent(func);
    ctor.SuppressDestruct();
    exports.Set("Scheduler", func);
    return func;
}
Node_Impl_Scheduler::Node_Impl_Scheduler(Napi::Value val)
    : m_obj(Napi::Persistent(val.ToObject()))
{
}
void Node_Impl_Scheduler::notify()
{
    auto env = m_obj.Env();
    return run_blocking_on_main_thread([&]() -> void {
        auto meth = m_obj.Get("notify");
        if (!meth.IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        auto ret = meth.As<Napi::Function>().MakeCallback(m_obj.Value(), {

                                                                         });
        return ((void)ret);
    });
}
bool Node_Impl_Scheduler::is_on_thread() const noexcept
{
    auto env = m_obj.Env();
    return run_blocking_on_main_thread([&]() -> bool {
        auto meth = m_obj.Get("is_on_thread");
        if (!meth.IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        auto ret = meth.As<Napi::Function>().MakeCallback(m_obj.Value(), {

                                                                         });
        return ret.ToBoolean().Value();
    });
}
bool Node_Impl_Scheduler::is_same_as(std::shared_ptr<Scheduler> const& other) const noexcept
{
    auto env = m_obj.Env();
    return run_blocking_on_main_thread([&]() -> bool {
        auto meth = m_obj.Get("is_same_as");
        if (!meth.IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        auto ret = meth.As<Napi::Function>().MakeCallback(m_obj.Value(), {NODE_FROM_SHARED_Scheduler(env, other)});
        return ret.ToBoolean().Value();
    });
}
bool Node_Impl_Scheduler::can_deliver_notifications() const noexcept
{
    auto env = m_obj.Env();
    return run_blocking_on_main_thread([&]() -> bool {
        auto meth = m_obj.Get("can_deliver_notifications");
        if (!meth.IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        auto ret = meth.As<Napi::Function>().MakeCallback(m_obj.Value(), {

                                                                         });
        return ret.ToBoolean().Value();
    });
}
void Node_Impl_Scheduler::set_notify_callback(std::function<void()> callback) noexcept
{
    auto env = m_obj.Env();
    return run_blocking_on_main_thread([&]() -> void {
        auto meth = m_obj.Get("set_notify_callback");
        if (!meth.IsFunction())
            throw Napi::TypeError::New(env, "expected a function");
        auto ret = meth.As<Napi::Function>().MakeCallback(
            m_obj.Value(),
            {

                (!(callback) ? env.Null()
                             : Napi::Function::New(env,
                                                   [func = std::forward<decltype(callback)>(callback)](
                                                       const Napi::CallbackInfo& info) -> Napi::Value {
                                                       auto env = info.Env();
                                                       if (info.Length() != 0)
                                                           throw Napi::TypeError::New(env, "expected 0 arguments");


                                                       func();
                                                       return env.Undefined();
                                                   }))

            });
        return ((void)ret);
    });
}
void makeNodeEnum_SchemaMode(Napi::Env env, Napi::Object exports)
{
    auto obj = Napi::Object::New(env);
    exports.Set("SchemaMode", obj);
    obj.Set("Automatic", 0);
    obj.Set("Immutable", 1);
    obj.Set("ReadOnlyAlternative", 2);
    obj.Set("ResetFile", 3);
    obj.Set("AdditiveDiscovered", 4);
    obj.Set("AdditiveExplicit", 5);
    obj.Set("Manual", 6);
}
SchemaMode nodeTo_SchemaMode(Napi::Env env, Napi::Value val)
{
    static_assert(sizeof(std::underlying_type_t<SchemaMode>) <= sizeof(int32_t));
    if (val.IsNumber()) {
        return SchemaMode(val.As<Napi::Number>().Int32Value());
    }

    auto it = strToEnum_SchemaMode.find(val.ToString().Utf8Value());
    if (it == strToEnum_SchemaMode.end())
        throw Napi::TypeError::New(env, "invalid string value for SchemaMode");
    return SchemaMode(it->second);
}
Napi::Value nodeFrom_SchemaMode(Napi::Env env, SchemaMode val)
{
    auto it = enumToStr_SchemaMode.find(int(val));
    if (it == enumToStr_SchemaMode.end())
        return Napi::Number::New(env, int(val));
    return Napi::String::New(env, it->second.data(), it->second.length());
}
void makeNodeEnum_PropertyType(Napi::Env env, Napi::Object exports)
{
    auto obj = Napi::Object::New(env);
    exports.Set("PropertyType", obj);
    obj.Set("Int", 0);
    obj.Set("Bool", 1);
    obj.Set("String", 2);
    obj.Set("Data", 3);
    obj.Set("Date", 4);
    obj.Set("Float", 5);
    obj.Set("Double", 6);
    obj.Set("Object", 7);
    obj.Set("LinkingObjects", 8);
    obj.Set("Mixed", 9);
    obj.Set("ObjectId", 10);
    obj.Set("Decimal", 11);
    obj.Set("UUID", 12);
    obj.Set("Required", 0);
    obj.Set("Nullable", 64);
    obj.Set("Array", 128);
    obj.Set("Set", 256);
    obj.Set("Dictionary", 512);
}
PropertyType nodeTo_PropertyType(Napi::Env env, Napi::Value val)
{
    static_assert(sizeof(std::underlying_type_t<PropertyType>) <= sizeof(int32_t));
    if (!val.IsNumber()) {
        throw Napi::TypeError::New(env, "Expected a number for PropertyType");
    }
    return PropertyType(val.As<Napi::Number>().Int32Value());
}
Napi::Value nodeFrom_PropertyType(Napi::Env env, PropertyType val)
{
    return Napi::Number::New(env, int(val));
}
Property nodeTo_Property(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    Property out = {};

    auto field_name = obj.Get("name");
    if (!field_name.IsUndefined()) {
        if constexpr (true)
            throw Napi::TypeError::New(env, "Property::name is required");
        out.name = field_name.ToString().Utf8Value();
    }

    auto field_public_name = obj.Get("public_name");
    if (!field_public_name.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::public_name is required");
        out.public_name = field_public_name.ToString().Utf8Value();
    }

    auto field_type = obj.Get("type");
    if (!field_type.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::type is required");
        out.type = nodeTo_PropertyType(env, field_type);
    }

    auto field_object_type = obj.Get("object_type");
    if (!field_object_type.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::object_type is required");
        out.object_type = field_object_type.ToString().Utf8Value();
    }

    auto field_link_origin_property_name = obj.Get("link_origin_property_name");
    if (!field_link_origin_property_name.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::link_origin_property_name is required");
        out.link_origin_property_name = field_link_origin_property_name.ToString().Utf8Value();
    }

    auto field_is_primary = obj.Get("is_primary");
    if (!field_is_primary.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::is_primary is required");
        out.is_primary = field_is_primary.ToBoolean().Value();
    }

    auto field_is_indexed = obj.Get("is_indexed");
    if (!field_is_indexed.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "Property::is_indexed is required");
        out.is_indexed = field_is_indexed.ToBoolean().Value();
    }

    auto field_column_key = obj.Get("column_key");
    if (!field_column_key.IsUndefined()) {
        if constexpr (true)
            throw Napi::TypeError::New(env, "Property::column_key is required");
        out.column_key = nodeTo_ColKey(env, field_column_key);
    }

    return out;
}
Napi::Value nodeFrom_Property(Napi::Env env, const Property& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("name", Napi::String::New(env, (obj.name).data(), (obj.name).size()));
    out.Set("public_name", Napi::String::New(env, (obj.public_name).data(), (obj.public_name).size()));
    out.Set("type", nodeFrom_PropertyType(env, (obj.type)));
    out.Set("object_type", Napi::String::New(env, (obj.object_type).data(), (obj.object_type).size()));
    out.Set("link_origin_property_name",
            Napi::String::New(env, (obj.link_origin_property_name).data(), (obj.link_origin_property_name).size()));
    out.Set("is_primary", Napi::Boolean::New(env, (obj.is_primary)));
    out.Set("is_indexed", Napi::Boolean::New(env, (obj.is_indexed)));
    out.Set("column_key", nodeFrom_ColKey(env, (obj.column_key)));
    return out;
}
VersionID nodeTo_VersionID(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    VersionID out = {};

    auto field_version = obj.Get("version");
    if (!field_version.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "VersionID::version is required");
        out.version = field_version.ToNumber().Int64Value();
    }

    auto field_index = obj.Get("index");
    if (!field_index.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "VersionID::index is required");
        out.index = field_index.ToNumber().Int32Value();
    }

    return out;
}
Napi::Value nodeFrom_VersionID(Napi::Env env, const VersionID& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("version", Napi::Number::New(env, (obj.version)));
    out.Set("index", Napi::Number::New(env, (obj.index)));
    return out;
}
ColKey nodeTo_ColKey(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    ColKey out = {};

    auto field_value = obj.Get("value");
    if (!field_value.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ColKey::value is required");
        out.value = field_value.ToNumber().Int64Value();
    }

    return out;
}
Napi::Value nodeFrom_ColKey(Napi::Env env, const ColKey& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("value", Napi::Number::New(env, (obj.value)));
    return out;
}
TableKey nodeTo_TableKey(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    TableKey out = {};

    auto field_value = obj.Get("value");
    if (!field_value.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "TableKey::value is required");
        out.value = field_value.ToNumber().Int32Value();
    }

    return out;
}
Napi::Value nodeFrom_TableKey(Napi::Env env, const TableKey& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("value", Napi::Number::New(env, (obj.value)));
    return out;
}
ObjectSchema nodeTo_ObjectSchema(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    ObjectSchema out = {};

    auto field_name = obj.Get("name");
    if (!field_name.IsUndefined()) {
        if constexpr (true)
            throw Napi::TypeError::New(env, "ObjectSchema::name is required");
        out.name = field_name.ToString().Utf8Value();
    }

    auto field_persisted_properties = obj.Get("persisted_properties");
    if (!field_persisted_properties.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::persisted_properties is required");
        out.persisted_properties = ([&] {
            if (!field_persisted_properties.IsArray())
                throw Napi::TypeError::New(env, "need an array");
            auto arr = field_persisted_properties.As<Napi::Array>();
            const size_t len = arr.Length();
            std::vector<Property> out;
            out.reserve(len);
            for (size_t i = 0; i < len; i++) {
                out.push_back(nodeTo_Property(env, arr[i]));
            }
            return out;
        }());
    }

    auto field_computed_properties = obj.Get("computed_properties");
    if (!field_computed_properties.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::computed_properties is required");
        out.computed_properties = ([&] {
            if (!field_computed_properties.IsArray())
                throw Napi::TypeError::New(env, "need an array");
            auto arr = field_computed_properties.As<Napi::Array>();
            const size_t len = arr.Length();
            std::vector<Property> out;
            out.reserve(len);
            for (size_t i = 0; i < len; i++) {
                out.push_back(nodeTo_Property(env, arr[i]));
            }
            return out;
        }());
    }

    auto field_primary_key = obj.Get("primary_key");
    if (!field_primary_key.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::primary_key is required");
        out.primary_key = field_primary_key.ToString().Utf8Value();
    }

    auto field_table_key = obj.Get("table_key");
    if (!field_table_key.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::table_key is required");
        out.table_key = nodeTo_TableKey(env, field_table_key);
    }

    auto field_is_embedded = obj.Get("is_embedded");
    if (!field_is_embedded.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::is_embedded is required");
        out.is_embedded = field_is_embedded.ToBoolean().Value();
    }

    auto field_alias = obj.Get("alias");
    if (!field_alias.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "ObjectSchema::alias is required");
        out.alias = field_alias.ToString().Utf8Value();
    }

    return out;
}
Napi::Value nodeFrom_ObjectSchema(Napi::Env env, const ObjectSchema& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("name", Napi::String::New(env, (obj.name).data(), (obj.name).size()));
    out.Set("persisted_properties", ([&] {
                auto&& arr = (obj.persisted_properties);
                const size_t len = arr.size();
                auto out = Napi::Array::New(env, len);
                for (size_t i = 0; i < len; i++) {
                    out[i] = nodeFrom_Property(env, arr[i]);
                }
                return out;
            }()));
    out.Set("computed_properties", ([&] {
                auto&& arr = (obj.computed_properties);
                const size_t len = arr.size();
                auto out = Napi::Array::New(env, len);
                for (size_t i = 0; i < len; i++) {
                    out[i] = nodeFrom_Property(env, arr[i]);
                }
                return out;
            }()));
    out.Set("primary_key", Napi::String::New(env, (obj.primary_key).data(), (obj.primary_key).size()));
    out.Set("table_key", nodeFrom_TableKey(env, (obj.table_key)));
    out.Set("is_embedded", Napi::Boolean::New(env, (obj.is_embedded)));
    out.Set("alias", Napi::String::New(env, (obj.alias).data(), (obj.alias).size()));
    return out;
}
RealmConfig nodeTo_RealmConfig(Napi::Env env, Napi::Value val)
{
    if (!val.IsObject())
        throw Napi::TypeError::New(env, "expected an object");
    auto obj = val.As<Napi::Object>();
    RealmConfig out = {};

    auto field_path = obj.Get("path");
    if (!field_path.IsUndefined()) {
        if constexpr (true)
            throw Napi::TypeError::New(env, "RealmConfig::path is required");
        out.path = field_path.ToString().Utf8Value();
    }

    auto field_fifo_files_fallback_path = obj.Get("fifo_files_fallback_path");
    if (!field_fifo_files_fallback_path.IsUndefined()) {
        if constexpr (true)
            throw Napi::TypeError::New(env, "RealmConfig::fifo_files_fallback_path is required");
        out.fifo_files_fallback_path = field_fifo_files_fallback_path.ToString().Utf8Value();
    }

    auto field_in_memory = obj.Get("in_memory");
    if (!field_in_memory.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "RealmConfig::in_memory is required");
        out.in_memory = field_in_memory.ToBoolean().Value();
    }

    auto field_schema = obj.Get("schema");
    if (!field_schema.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "RealmConfig::schema is required");
        out.schema = (field_schema.IsUndefined() ? util::none : util::Optional<std::vector<ObjectSchema>>(([&] {
            if (!field_schema.IsArray())
                throw Napi::TypeError::New(env, "need an array");
            auto arr = field_schema.As<Napi::Array>();
            const size_t len = arr.Length();
            std::vector<ObjectSchema> out;
            out.reserve(len);
            for (size_t i = 0; i < len; i++) {
                out.push_back(nodeTo_ObjectSchema(env, arr[i]));
            }
            return out;
        }())));
    }

    auto field_schema_version = obj.Get("schema_version");
    if (!field_schema_version.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "RealmConfig::schema_version is required");
        out.schema_version = uint64_t(field_schema_version.ToNumber().Int64Value());
    }

    auto field_initialization_function = obj.Get("initialization_function");
    if (!field_initialization_function.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "RealmConfig::initialization_function is required");
        out.initialization_function = ([&] {
            if (!field_initialization_function.IsFunction())
                throw Napi::TypeError::New(env, "expected a function");
            return [func = std::make_shared<Napi::FunctionReference>(Napi::Persistent(
                        field_initialization_function.As<Napi::Function>()))](std::shared_ptr<Realm> realm) {
                auto env = func->Env();
                return run_blocking_on_main_thread([&]() -> void {
                    auto ret = func->MakeCallback(env.Undefined(), {NODE_FROM_SHARED_Realm(env, realm)});
                    return ((void)ret);
                });
            };
        }());
    }

    auto field_should_compact_on_launch_function = obj.Get("should_compact_on_launch_function");
    if (!field_should_compact_on_launch_function.IsUndefined()) {
        if constexpr (false)
            throw Napi::TypeError::New(env, "RealmConfig::should_compact_on_launch_function is required");
        out.should_compact_on_launch_function = ([&] {
            if (!field_should_compact_on_launch_function.IsFunction())
                throw Napi::TypeError::New(env, "expected a function");
            return [func = std::make_shared<Napi::FunctionReference>(
                        Napi::Persistent(field_should_compact_on_launch_function.As<Napi::Function>()))](
                       uint64_t total_bytes, uint64_t used_bytes) {
                auto env = func->Env();
                return run_blocking_on_main_thread([&]() -> bool {
                    auto ret = func->MakeCallback(
                        env.Undefined(), {Napi::Number::New(env, total_bytes), Napi::Number::New(env, used_bytes)});
                    return ret.ToBoolean().Value();
                });
            };
        }());
    }

    return out;
}
Napi::Value nodeFrom_RealmConfig(Napi::Env env, const RealmConfig& obj)
{
    auto out = Napi::Object::New(env);
    out.Set("path", Napi::String::New(env, (obj.path).data(), (obj.path).size()));
    out.Set("fifo_files_fallback_path",
            Napi::String::New(env, (obj.fifo_files_fallback_path).data(), (obj.fifo_files_fallback_path).size()));
    out.Set("in_memory", Napi::Boolean::New(env, (obj.in_memory)));
    out.Set("schema", (!((obj.schema)) ? env.Null() : (([&] {
                auto&& arr = (*(obj.schema));
                const size_t len = arr.size();
                auto out = Napi::Array::New(env, len);
                for (size_t i = 0; i < len; i++) {
                    out[i] = nodeFrom_ObjectSchema(env, arr[i]);
                }
                return out;
            }()))));
    out.Set("schema_version", Napi::Number::New(env, (obj.schema_version)));
    out.Set("initialization_function",
            (!((obj.initialization_function))
                 ? env.Null()
                 : Napi::Function::New(
                       env,
                       [func = std::forward<decltype((obj.initialization_function))>((obj.initialization_function))](
                           const Napi::CallbackInfo& info) -> Napi::Value {
                           auto env = info.Env();
                           if (info.Length() != 1)
                               throw Napi::TypeError::New(env, "expected 1 arguments");


                           func(NODE_TO_SHARED_Realm(info[0]));
                           return env.Undefined();
                       })));
    out.Set("should_compact_on_launch_function",
            (!((obj.should_compact_on_launch_function))
                 ? env.Null()
                 : Napi::Function::New(
                       env,
                       [func = std::forward<decltype((obj.should_compact_on_launch_function))>(
                            (obj.should_compact_on_launch_function))](const Napi::CallbackInfo& info) -> Napi::Value {
                           auto env = info.Env();
                           if (info.Length() != 2)
                               throw Napi::TypeError::New(env, "expected 2 arguments");

                           auto&& val = func(uint64_t(info[0].ToNumber().Int64Value()),
                                             uint64_t(info[1].ToNumber().Int64Value()));
                           return Napi::Boolean::New(env, val);
                       })));
    return out;
}
const std::shared_ptr<Transaction>& NODE_TO_SHARED_Transaction(Napi::Value val)
{
    return Node_Transaction::Unwrap(val.ToObject())->m_ptr;
}
Napi::Value NODE_FROM_SHARED_Transaction(Napi::Env env, std::shared_ptr<Transaction> ptr)
{
    return Node_Transaction::ctor.New({Napi::External<std::shared_ptr<Transaction>>::New(env, &ptr)});
}
const ObjectStore& NODE_TO_CLASS_ObjectStore(Napi::Value val)
{
    return Node_ObjectStore::Unwrap(val.ToObject())->m_val;
}
Napi::Value NODE_FROM_CLASS_ObjectStore(Napi::Env env, ObjectStore val)
{
    return Node_ObjectStore::ctor.New({Napi::External<ObjectStore>::New(env, &val)});
}
const std::shared_ptr<Realm>& NODE_TO_SHARED_Realm(Napi::Value val)
{
    return Node_Realm::Unwrap(val.ToObject())->m_ptr;
}
Napi::Value NODE_FROM_SHARED_Realm(Napi::Env env, std::shared_ptr<Realm> ptr)
{
    return Node_Realm::ctor.New({Napi::External<std::shared_ptr<Realm>>::New(env, &ptr)});
}
std::shared_ptr<Scheduler> NODE_TO_INTERFACE_Scheduler(Napi::Value val)
{
    // Don't double-wrap if already have a wrapped implementation.
    if (val.ToObject().InstanceOf(Node_Scheduler::ctor.Value()))
        return NODE_TO_SHARED_Scheduler(val);
    return std::make_shared<Node_Impl_Scheduler>(val);
}
const std::shared_ptr<Scheduler>& NODE_TO_SHARED_Scheduler(Napi::Value val)
{
    return Node_Scheduler::Unwrap(val.ToObject())->m_ptr;
}
Napi::Value NODE_FROM_SHARED_Scheduler(Napi::Env env, std::shared_ptr<Scheduler> ptr)
{
    return Node_Scheduler::ctor.New({Napi::External<std::shared_ptr<Scheduler>>::New(env, &ptr)});
}
Napi::Object moduleInit(Napi::Env env, Napi::Object exports)
{
    makeNodeEnum_SchemaMode(env, exports);
    makeNodeEnum_PropertyType(env, exports);
    Node_Transaction::init(env, exports);
    Node_ObjectStore::init(env, exports);
    Node_Realm::init(env, exports);
    Node_Scheduler::init(env, exports);
    return exports;
}

NODE_API_MODULE(realm_cpp, moduleInit)
} // namespace realm::js::node
