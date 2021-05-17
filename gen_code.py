import builtins
import itertools
from apm import *
import yaml
from typing import NamedTuple, Callable
import typing
import re
import sys

from textwrap import dedent
from contextlib import contextmanager
from itertools import chain

from cparser import *


with open('types.yml') as f:
    tree = yaml.safe_load(f)


def yamlFunc(name, func, out_list):
     match(func,
        str, lambda: out_list.append(Method(name, name, parse(func, syms))),
        dict(suffix=_, sig=_), lambda suffix, sig: out_list.append(Method(name, f'{name}_{suffix}', parse(sig, syms))),
        list, lambda: [yamlFunc(name, f, out_list) for f in func]
     )

# Set up syms with all type names
syms = SymbolTable()
for name in tree.get('primitives', ()):
    syms.addType(name, Primitive(name))

for name, cls_raw in tree.get('classes', {}).items():
    cls = Class(name)
    syms.classes.append(cls)
    syms.addType(name, cls)

    if sharedName := cls_raw.get('sharedPtrWrapped'):
        syms.addType(sharedName, Shared(cls))
        cls.shared_ptr_wrapped = True

for name in tree.get('templates', ()):
    syms.templates[name] = Template(parseQualName(name))

for subtree, ctor in (('enums', Enum), ('records', Record)):
    for name in tree.get(subtree, ()):
        syms.addType(name, ctor(name))

for name in tree.get('opaque_types', ()):
    syms.addType(name, Opaque(name))

for dest, src in tree['type_aliases'].items():
    syms.addType(dest, parse(src, syms))

# Now consume syms and bind names

for enum_name, enum_raw in tree.get('enums', {}).items():
    enum = syms.types[enum_name]
    syms.enums.append(enum)

    if type(enum_raw['values']) == list:
        enum_values = {}
        for i, name in enumerate(enum_raw['values']):
            enum_values[name] = i
        enum_raw['values'] = enum_values

    for name, val in enum_raw['values'].items():
        enum.enumerators.append(Enumerator(name, val))
    if is_flag := enum_raw.get('is_flag'):
        enum.is_flag = True
    if (flag_mask := enum_raw.get('flag_mask')) is not None:
        enum.flag_mask = flag_mask

for rec_name, rec_raw in tree.get('records', {}).items():
    rec = syms.types[rec_name]
    syms.records.append(rec)
    for name, info in rec_raw.get('fields', {}).items():
        rec.fields.append(match(
            info,
            str, lambda: Field(name, parse(info, syms)),
            {type: _}, lambda t: Field(name, parse(info['type'], syms), info.get('default')),
        ))

for cls_name, cls_raw in tree.get('classes', {}).items():
    cls = syms.types[cls_name]

    cls.properties = {p: parse(t, syms) for p, t in cls_raw.get('properties', {}).items()}

    for name, func in cls_raw.get('methods', {}).items():
        yamlFunc(name, func, cls.methods)

    for name, func in cls_raw.get('staticMethods', {}).items():
        yamlFunc(name, func, cls.static_methods)


@dataclass
class CppVar:
    type: str
    name: str
    default: str = ''
    static: bool = False

    def arg_definition(self):
        return f'{self.type} {self.name}'

    def arg_declaration(self):
        default = ''
        if self.default:
            default = f' = {self.default}'
        return f'{self.type} {self.name}{default}'

    def definition(self):
        static = 'static' if self.static else ''
        return f'{static} {self.arg_declaration()};'

@dataclass
class CppFunc:
    name: str
    ret: str
    args: list[CppVar]
    body: str = ''
    static: bool = False
    const: bool = False

    def declaration(self):
        static = 'static' if self.static else ''
        const = 'const' if self.const else ''
        return f'{static} {self.ret} {self.name}({", ".join(a.arg_declaration() for a in self.args)}) {const};'

    def definition(self):
        # TODO add optional try/catch wrapping
        const = 'const' if self.const else ''
        return f'{self.ret} {self.qualName()}({", ".join(a.arg_definition() for a in self.args)}) {const} {{ {self.body} }}'

    def qualName(self): return self.name

@dataclass
class CppMemInit:
    name: str
    val: str

    def init(self):
        return f'{self.name}({self.val})'

@dataclass
class CppMethod(CppFunc):
    on: Any = None
    def qualName(self): return f'{self.on.name}::{self.name}'


@dataclass
class CppCtor(CppMethod):
    mem_inits: list[CppMemInit] = emptyListField()

    def definition(self):
        assert not self.ret
        assert not self.const
        assert not self.static

        mem_inits = f': {", ".join(m.init() for m in self.mem_inits)}' if self.mem_inits else ''
        return f'{self.qualName()}({", ".join(a.arg_definition() for a in self.args)}) {mem_inits} {{ {self.body} }}'


node_callback_info = CppVar('const Napi::CallbackInfo&', 'info')

def CppNodeMethod(name, **kwargs):
        return CppMethod(name, 'Napi::Value', [node_callback_info], **kwargs)

class CppClass:
    def __init__(self, name):
        self.name = name
        self.methods = []
        self.members = []
        self.bases = []

    def addMethod(self, meth):
        meth.on = self
        self.methods.append(meth)
        return meth

    def fwdDecl(self):
        return f'class {self.name};'

    def definition(self):
        base = ''
        if self.bases:
            base = ', '.join(f'public {b}' for b in self.bases)
            base = f' : {base} '

        return dedent(f'''
            class {self.name}{base}{{
            public:
                {' '.join(m.declaration() for m in self.methods)}
                {' '.join(m.definition() for m in self.members)}
            }};''')

    def methodDefs(self):
        return '\n'.join(m.definition() for m in self.methods)


free_funcs = []
free_vars = []
classes = []
inits = []


def Templ(name, *args): return TemplateInstance(syms.templates[name], list(args))

for enum in syms.enums:
    free_vars.append(CppVar(
        'const std::unordered_map<std::string_view, int>',
        f'strToEnum_{enum.name}',
        '{%s}' % ', '.join(f'{{ "{e.name}", {e.value} }}' for e in enum.enumerators)))
    free_vars.append(CppVar(
        'const std::unordered_map<int, std::string_view>',
        f'enumToStr_{enum.name}',
        '{%s}' % ', '.join(f'{{ {e.value}, "{e.name}" }}' for e in enum.enumerators)))

    free_funcs.append(CppFunc(
        f'makeNodeEnum_{enum.name}',
        'void',
        [CppVar('Napi::Env', 'env'), CppVar('Napi::Object', 'exports')],
        body=dedent(f'''
            auto obj = Napi::Object::New(env);
            exports.Set("{enum.name}", obj);
            {' '.join(f'obj.Set("{e.name}", {e.value});' for e in enum.enumerators)}
        '''),
    ))
    inits.append(f'makeNodeEnum_{enum.name}')

    if not enum.is_flag:
        free_funcs.append(CppFunc(
            f'nodeTo_{enum.name}',
            enum.name,
            [CppVar('Napi::Env', 'env'), CppVar('Napi::Value', 'val')],
            body = dedent(f'''
                static_assert(sizeof(std::underlying_type_t<{enum.name}>) <= sizeof(int32_t));
                if (val.IsNumber()) {{
                    return {enum.name}(val.As<Napi::Number>().Int32Value());
                }}

                auto it = strToEnum_{enum.name}.find(val.ToString().Utf8Value());
                if (it == strToEnum_{enum.name}.end())
                    throw Napi::TypeError::New(env, "invalid string value for {enum.name}");
                return {enum.name}(it->second);
                ''')))

        free_funcs.append(CppFunc(
            f'nodeFrom_{enum.name}',
            'Napi::Value',
            [CppVar('Napi::Env', 'env'), CppVar(enum.name, 'val')],
            body = dedent(f'''
                auto it = enumToStr_{enum.name}.find(int(val));
                if (it == enumToStr_{enum.name}.end())
                    return Napi::Number::New(env, int(val));
                return Napi::String::New(env, it->second.data(), it->second.length());
                ''')))
    else:
        # For now, just round-tripping as numbers.
        # TODO some stringification?
        free_funcs.append(CppFunc(
            f'nodeTo_{enum.name}',
            enum.name,
            [CppVar('Napi::Env', 'env'), CppVar('Napi::Value', 'val')],
            body = dedent(f'''
                static_assert(sizeof(std::underlying_type_t<{enum.name}>) <= sizeof(int32_t));
                if (!val.IsNumber()) {{
                    throw Napi::TypeError::New(env, "Expected a number for {enum.name}");
                }}
                return {enum.name}(val.As<Napi::Number>().Int32Value());
                ''')))

        free_funcs.append(CppFunc(
            f'nodeFrom_{enum.name}',
            'Napi::Value',
            [CppVar('Napi::Env', 'env'), CppVar(enum.name, 'val')],
            body = 'return Napi::Number::New(env, int(val));',
        ))

def convertPrimToNode(kind, expr):
    numbers = ['double', 'int'] + [f'{u}int{sz}_t' for u in ('u', '') for sz in ('32', '64')]
    return match(
        kind,
        'void', lambda: f'env.Undefined()',
        'bool', lambda: f'Napi::Boolean::New(env, {expr})',
        OneOf(*numbers), lambda: f'Napi::Number::New(env, {expr})',
        OneOf('std::string', 'StringData'), lambda: f"Napi::String::New(env, {expr}.data(), {expr}.size())",
        'OwnedBinaryData', lambda: convertPrimToNode('BinaryData', f'{expr}.get()'),
        'BinaryData', lambda: f'''([&] {{
                auto arr = Napi::ArrayBuffer::New(env, {expr}.size());
                memcpy(arr.Data(), {expr}.data(), {expr}.size());
                return arr;
            }}())''',
    )

def convertToNode(type, expr):
    c = convertToNode # shorthand for recursion
    return match(
        type,

        Primitive(_), lambda type: convertPrimToNode(type, expr),
        OneOf(Enum, Record), lambda: f"nodeFrom_{type.name}(env, {expr})",
        Templ('util::Optional', _), lambda type: f"(!({expr}) ?  env.Null() : ({c(type, f'(*{expr})')}))",
        Templ('std::vector', _), lambda type: f'''
            ([&] {{
                auto&& arr = {expr};
                const size_t len = arr.size();
                auto out = Napi::Array::New(env, len);
                for (size_t i = 0; i < len; i++) {{
                    out[i] = {c(type, 'arr[i]')};
                }}
                return out;
            }}())
        ''',

        FuncSig(_, _), lambda ret, args: 'env.Undefined()', # TODO? convert std::function to node function

        Shared(_), lambda type: f"NODE_FROM_SHARED_{type.name}(env, {expr})",

        Class, lambda: f"NODE_FROM_CLASS_{type.name}({expr})",

        Opaque(_), lambda name: f'Napi::External<{name}>::New(env, &{expr})',

        Ref(_), lambda type: c(type, expr),
        Const(_), lambda type: c(type, expr),
        Pointer(_), lambda type: c(type, '*' + expr),
    )

def convertPrimFromNode(kind, expr):
    numbers = ['double'] + [f'{u}int{sz}_t' for u in ('u', '') for sz in ('32', '64')]
    return match(
        kind,
        'void', lambda: f'((void){expr})',
        'bool', lambda: f'{expr}.ToBoolean().Value()',
        'bool', lambda: f'{expr}.ToBoolean().Value()',
        'int32_t', lambda: f'{expr}.ToNumber().Int32Value()',
        'uint32_t', lambda: f'{expr}.ToNumber().UInt32Value()',
        'int64_t', lambda: f'{expr}.ToNumber().Int64Value()', #XXX
        'uint64_t', lambda: f'uint64_t({expr}.ToNumber().Int64Value())', #XXX
        'double', lambda: f'{expr}.ToNumber().DoubleValue()',
        OneOf('std::string', 'StringData'), lambda: f"{expr}.ToString().Utf8Value()",
        'BinaryData', lambda: f'''([&] {{
                if (!{expr}.IsArrayBuffer())
                    throw Napi::TypeError::New(env, "expected ArrayBuffer");
                auto buf = {expr}.As<Napi::ArrayBuffer>();
                return BinaryData(static_cast<const char*>(buf.Data()), buf.ByteLength());
            }}())'''
    )

def cppType(type):
    c = cppType
    return match(
        type,
        Primitive, lambda: type.name,
        #QualName, lambda: str(type),
        Class, lambda type: type.name,
        Record, lambda type: type.name,
        Shared(_), lambda type: f"std::shared_ptr<{c(type)}>",
        Pointer(_), lambda type: c(type) + '*',
        Const(_), lambda type: c(type) + ' const',
        Ref(_), lambda type: c(type) + '&',
        RRef(_), lambda type: c(type) + 'R&',
        TemplateInstance(Template(QualName(_)), _), lambda tmpl, args: f"{'::'.join(tmpl)}<{','.join(c(a) for a in args)}>",
    )

def convertFuncFromNode(expr, ret, args):
    return f'''([&] {{
                if (!{expr}.IsFunction())
                    throw Napi::TypeError::New(env, "expected a function");
                return [
                    func = std::make_shared<Napi::FunctionReference>(Napi::Persistent({expr}.As<Napi::Function>()))
                ] ({', '.join(f'{cppType(a.type)} {a.name}' for a in args)}) {{
                    auto env = func->Env();
                    return run_blocking_on_main_thread([&] () -> {cppType(ret)} {{
                        auto ret = func->MakeCallback(
                            env.Undefined(),
                            {{
                                {', '.join(f'{convertToNode(a.type, a.name)}' for a in args)}
                            }});
                        return {convertFromNode(ret, 'ret')};
                    }});
                }};
            }}())'''

def convertFromNode(type, expr):
    c = convertFromNode # shorthand for recursion
    return match(
        type,

        Primitive(_), lambda type: convertPrimFromNode(type, expr),
        FuncSig(_, _), lambda ret, args: convertFuncFromNode(expr, ret, args),
        OneOf(Enum, Record), lambda: f"nodeTo_{type.name}(env, {expr})",
        Templ('util::Optional', _), lambda type: f"({expr}.IsUndefined() ? util::none : util::Optional<{cppType(type)}>({c(type, expr)}))",
        Templ('std::vector', _), lambda type: f'''
            ([&] {{
                if (!{expr}.IsArray())
                    throw Napi::TypeError::New(env, "need an array");
                auto arr = {expr}.As<Napi::Array>();
                const size_t len = arr.Length();
                std::vector<{type.name}> out;
                out.reserve(len);
                for (size_t i = 0; i < len; i++) {{
                    out.push_back({c(type, 'arr[i]')});
                }}
                return out;
            }}())
        ''',


        Shared(_), lambda type: f"NODE_TO_SHARED_{type.name}({expr})",

        Class, lambda: f"NODE_TO_CLASS_{type.name}({expr})",

        Opaque(_), lambda name: f'*({expr}.As<Napi::External<{name}>>().Data())',

        # TODO?
        Ref(_), lambda type: c(type, expr),
        Const(_), lambda type: c(type, expr),
        Pointer(_), lambda type: f'&({c(type, expr)})',
    )

for rec in syms.records:
    fields_from = []
    fields_to = []
    for field in rec.fields:
        fields_from.append(f'out.Set("{field.name}", {convertToNode(field.type, f"(obj.{field.name})")});')
        fields_to.append(f'''
            auto field_{field.name} = obj.Get("{field.name}");
            if (!field_{field.name}.IsUndefined()) {{
                if constexpr ({'true' if field.default is None else 'false'})
                    throw Napi::TypeError::New(env, "{rec.name}::{field.name} is required");
                out.{field.name} = {convertFromNode(field.type, f'field_{field.name}')};
            }}
        ''')


    free_funcs.append(CppFunc(
        f'nodeTo_{rec.name}',
        rec.name,
        [CppVar('Napi::Env', 'env'), CppVar('Napi::Value', 'val')],
        body = dedent(f'''
            if (!val.IsObject())
                throw Napi::TypeError::New(env, "expected an object");
            auto obj = val.As<Napi::Object>();
            {rec.name} out = {{}};
            {''.join(fields_to)}
            return out;
            ''')))

    free_funcs.append(CppFunc(
        f'nodeFrom_{rec.name}',
        'Napi::Value',
        [CppVar('Napi::Env', 'env'), CppVar(f'const {rec.name}&', 'obj')],
        body = dedent(f'''
            auto out = Napi::Object::New(env);
            {''.join(fields_from)}
            return out;
            ''')))

for cls in syms.classes:
    # TODO map name styles from core-cpp to js
    cpp_cls = CppClass(f'Node_{cls.name}')
    classes.append(cpp_cls)
    cpp_cls.bases.append(f'Napi::ObjectWrap<{cpp_cls.name}>')
    cpp_cls.members.append(CppVar('Napi::FunctionReference', 'ctor', static=True))


    ctor = cpp_cls.addMethod(CppCtor(cpp_cls.name, '', [node_callback_info],
        mem_inits = [CppMemInit(f'Napi::ObjectWrap<{cpp_cls.name}>', 'info')]))

    if cls.shared_ptr_wrapped:
        ptr = f'std::shared_ptr<{cls.name}>'
        free_funcs.append(CppFunc(
            f'NODE_TO_SHARED_{cls.name}',
            f'const {ptr}&',
            [CppVar('Napi::Value', 'val')],
            body = dedent(f'''
                return {cpp_cls.name}::Unwrap(val.ToObject())->m_ptr;
            '''),
        ))
        free_funcs.append(CppFunc(
            f'NODE_FROM_SHARED_{cls.name}',
            'Napi::Value',
            [CppVar('Napi::Env', 'env'), CppVar(f'const {ptr}&', 'ptr')],
            body = dedent(f'''
                return {cpp_cls.name}::ctor.New({{Napi::External<{ptr}>::New(env, const_cast<{ptr}*>(&ptr))}});
            '''),
        ))
        obj = '(*m_ptr)'
    else:
        free_funcs.append(CppFunc(
            f'NODE_TO_SHARED_{cls.name}',
            f'const {cls.name}&',
            [CppVar('Napi::Value', 'val')],
            body = dedent(f'''
                return {cpp_cls.name}::Unwrap(val.ToObject())->m_val;
            '''),
        ))
        free_funcs.append(CppFunc(
            f'NODE_FROM_SHARED_{cls.name}',
            'Napi::Value',
            [CppVar('Napi::Env', 'env'), CppVar(f'const {cls.name}&', 'val')],
            body = dedent(f'''
                return {cpp_cls.name}::ctor.New({{Napi::External<{cls.name}>::New(env, const_cast<{cls.name}*>(&val))}});
            '''),
        ))
        obj = '(m_val)'


    descriptors = []
    for method in cls.methods:
        cpp_meth = cpp_cls.addMethod(CppNodeMethod('meth_'+method.unique_name))
        descriptors.append(f'InstanceMethod<&{cpp_meth.qualName()}>("{method.unique_name}")')
        ret = method.type.ret
        args = method.type.args
        ret_decl = 'auto&& val = ' if ret != Primitive('void') else ''
        cpp_meth.body = dedent(f'''
            auto env = info.Env();
            if (info.Length() != {len(args)})
                throw Napi::TypeError::New(env, "expected {len(args)} arguments");

            {ret_decl}{obj}.{method.name}({', '.join(convertFromNode(arg.type, f'info[{i}]') for i, arg in enumerate(args))});
            return {convertToNode(ret, 'val')};
            ''')
    for method in cls.static_methods:
        cpp_meth = cpp_cls.addMethod(CppNodeMethod('meth_'+method.unique_name, static=True))
        descriptors.append(f'StaticMethod<&{cpp_meth.qualName()}>("{method.unique_name}")')
        ret = method.type.ret
        ret_decl = 'auto&& val = ' if ret != Primitive('void') else ''
        args = method.type.args
        cpp_meth.body = dedent(f'''
            auto env = info.Env();
            if (info.Length() != {len(args)})
                throw Napi::TypeError::New(env, "expected {len(args)} arguments");

            {ret_decl}{cls.name}::{method.name}({', '.join(convertFromNode(arg.type, f'info[{i}]') for i, arg in enumerate(args))});
            return {convertToNode(ret, 'val')};
            ''')
    for prop, type in cls.properties.items():
        cpp_meth = cpp_cls.addMethod(CppNodeMethod('prop_'+prop))
        descriptors.append(f'InstanceAccessor<&{cpp_meth.qualName()}>("{prop}")')
        cpp_meth.body = dedent(f'''
            auto env = info.Env();
            auto&& val = {obj}.{prop}();
            return {convertToNode(type, 'val')};
            ''')

    ctor.body = dedent('''
        auto env = info.Env();
        if (info.Length() != 1 || !info[0].IsExternal())
            throw Napi::TypeError::New(env, "need 1 external argument");
    ''')

    if cls.shared_ptr_wrapped:
        cpp_cls.members.append(CppVar(ptr, 'm_ptr'))
        ctor.body += f'm_ptr = std::as_const(*info[0].As<Napi::External<{ptr}>>().Data());'
    else:
        cpp_cls.members.append(CppVar(cls.name, 'm_val'))
        ctor.body += f'm_val = std::as_const(*info[0].As<Napi::External<{cls.name}>>().Data());'

    init = cpp_cls.addMethod(CppMethod(
        'init',
        'Napi::Function',
        [CppVar('Napi::Env', 'env'), CppVar('Napi::Object', 'exports')],
        static = True,
        body = dedent(f'''
            auto func = DefineClass(env, "{cls.name}", {{ {", ".join(descriptors)} }});
            ctor = Napi::Persistent(func);
            ctor.SuppressDestruct();
            exports.Set("{cls.name}", func);
            return func;
            '''),
    ))
    inits.append(init.qualName())

free_funcs.append(CppFunc(
    'moduleInit',
    'Napi::Object',
    [CppVar('Napi::Env', 'env'), CppVar('Napi::Object', 'exports')],
    body='\n'.join(f'{func}(env, exports);' for func in inits) + '\n return exports;'
))


needed_headers = tree.get('headers', []) + [
    'type_traits',
    'future',
    'functional',
    'queue',
    'unordered_map',

    'napi.h',

    'realm/object-store/util/scheduler.hpp',
]

for header in needed_headers:
    print(f'#include "{header}"')

print('namespace realm::js::node {')

#TODO hacks...
print('using RealmConfig = Realm::Config;')


#TODO move to support header file or something.
print('''
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

void run_async_on_main_thread(std::function<void()> f) {
    scheduler_queue.push(std::move(f));
    scheduler->notify();
}

template <typename F, typename = std::enable_if_t<std::is_invocable_r_v<void, F>>>
void run_maybe_async_on_main_thread(F&& f) {
    if (scheduler->is_on_thread()) {
        return std::forward<F>(f)();
    }
    run_async_on_main_thread(std::forward<F>(f));
}

template <typename F, typename Ret = decltype(std::declval<F>()())>
Ret run_blocking_on_main_thread(F&& f) {
    if (scheduler->is_on_thread()) {
        return std::forward<F>(f)();
    }

    auto task = std::make_shared<std::packaged_task<Ret()>>(std::forward<F>(f));
    auto fut = task->get_future();
    run_async_on_main_thread([task=std::move(task)] { (*task)(); });
    return fut.get();
}
}
''')

for cls in classes: print(cls.fwdDecl())
for func in free_funcs: print(func.declaration())
for var in free_vars: print(var.definition())
for cls in classes: print(cls.definition())
for cls in classes: print(cls.methodDefs())
for func in free_funcs: print(func.definition())

print('''
NODE_API_MODULE(realm_cpp, moduleInit)
} // namespace realm::js::node
''')


sys.exit()



'''
cApiStyle = Style(
    func = snake_case,
    type = PascalCase,
    var = camelCase,
    enum = SHOUT_CASE,
    namespaceJoin = '_'.join,
    canOverload = True,
)

def toCType(t, name = ''):
    c = toCType # shorthand for recursion
    return match(
        t,

        Pointer(_), lambda t: f"{c(t)}* {name}",
        Ref(_), lambda t: f"{c(t)}* {name}",
        RRef(_), lambda t: f"{c(t)}* {name}",
        Const(_), lambda t: f"{c(t)} const {name}",

        #user data
        FuncSig(_, _, _), lambda ret, args, const: f"{c(ret)}(*{name})({', '.join(c(arg.type, arg.name) for arg in args)})",

        Primitive(_), lambda t: f"{t} {name}",

        # TODO...
        Shared(_), lambda t: f"{c(t)} shr {name}",
        InstanceOf(Class), lambda: f"{t.name} todocls {name}",
        AllOf(str, Check(lambda t: t in syms.types)), lambda: f"{t} __TODO__ {name}",
        TemplateInstance(Template(QualName(['util', 'Optional'])), [_]), lambda t: f"{c(t)} __opt__ * {name}",
    )

for cls in syms.classes:
    for method in cls.methods:
        ret = toCType(method.type.ret)
        this = Arg('this', Const.maybe(Pointer(cls.name), method.type.const))
        args = ', '.join(toCType(arg.type, arg.name) for arg in chain([this], method.type.args))
        print(f"{ret} realm_{cls.name}_{method.unique_name}({args})")

sys.exit()




for cls_name, cls in tree['classes'].items():
    for name, func in cls.get('methods', {}).items():
        if isinstance(func, str):
            cls['methods'][name] = parse(func)

outfile = 'type_checks.cpp'
output = open(outfile, 'w')

for header in tree['headers']:
    output.write(f'#include "{header}"\n')

for header in ('type_traits', ):
    output.write(f'#include "{header}"\n')

output.write('namespace realm {\n');

for alias, type in tree['type_aliases'].items():
    output.write(f'static_assert(std::is_same_v<{alias}, {type}>);\n')

for cls_name, cls in tree['classes'].items():
    if 'sharedPtrWrapped' in cls:
        output.write(f'static_assert(std::is_same_v<{cls["sharedPtrWrapped"]}, std::shared_ptr<{cls_name}>>);\n')

    for name, type in cls.get('properties', {}).items():
        obj = f'std::declval<const {cls_name}>()'
        output.write(f'static_assert(std::is_same_v<decltype({obj}.{name}()), {type}>);\n')

    for name, func in cls.get('methods', {}).items():
        if not isinstance(func, str):
            print(f'ignoring {cls_name}::{name}')
            continue
        sig = parse(func)
        obj = f'std::declval<{"const" if sig.const else ""} {cls_name}>()'
        args = ', '.join(f'std::declval<{arg.type}>()' for arg in sig.args)
        output.write(f'static_assert(std::is_same_v<decltype({obj}.{name}({args})), {sig.ret}>);\n')


output.write('} // namespace realm\n')
output.close()
            '''
