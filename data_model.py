from typing import Any, Dict, Generic, List, NamedTuple, Text, TypeVar, Union, Callable
from dataclasses import dataclass, field
from apm import match, _

from contextlib import contextmanager

Type = Any

CaseFunc = Callable[[List[str]], str]

def emptyListField(): return field(default_factory=list)
def emptyDictField(): return field(default_factory=dict)

def camelCase(name: List[str]):
    out = [name[0].lower()]
    for part in name[1:]:
        out.append(part.title())
    return ''.join(out)

def PascalCase(name: List[str]):
    return ''.join(part.title() for part in name)

def snake_case(name: List[str]):
    return '_'.join(part.lower() for part in name)

def SHOUT_CASE(name: List[str]):
    return '_'.join(part.upper() for part in name)

class Style(NamedTuple):
    type: CaseFunc
    func: CaseFunc
    var: CaseFunc
    enum: CaseFunc

    namespaceJoin: Callable[[List[str]], str]
    canOverload: bool

coreCppStyle = Style(
    func = camelCase,
    type = PascalCase,
    var = camelCase,
    enum = SHOUT_CASE,
    namespaceJoin = '::'.join,
    canOverload = True,
)

currentStyle = coreCppStyle

@contextmanager
def withStyle(style):
    oldStyle = currentStyle
    currentStyle = style
    try:
        yield
    finally:
        currentStyle = oldStyle

def splitName(name):
    name = re.sub(r'([a-z])([A-Z])', r'\1_\2', name)
    name = name.lower()
    return name.split('_')

class QualName(NamedTuple):
    names: List[str]

    def __str__(self):
        return coreCppStyle.namespaceJoin(self.names)

    def __eq__(self, other):
         return match(
            other,
            str, lambda: str(self) == other,
            QualName(_), lambda names: self.names == names,
            _, lambda x: False
        )

    def __hash__(self):
        return hash(str(self))


@dataclass
class WrapperType:
    @property
    def isWrapper(self): return True

    def visitTypes(self, visitor):
        visitor(self.type)

@dataclass
class Const(WrapperType):
    type: Type

    @staticmethod
    def maybe(type, isConst):
        return Const(type) if isConst else type

    def __str__(self):
        return f'{self.type} const'

@dataclass
class Pointer(WrapperType):
    type: Type

    def __str__(self): return f'{self.type}*'

@dataclass
class Ref(WrapperType):
    type: Type

    def __str__(self): return f'{self.type}&'

@dataclass
class RRef(WrapperType):
    type: Type

    def __str__(self): return f'{self.type}&&'

@dataclass
class Arg(WrapperType):
    name: str
    type: Type

    def __str__(self):
        return f'{self.name}: {self.type}'


@dataclass
class FuncSig:
    ret: Type
    args: List[Arg]
    const: bool = False
    noexcept: bool = False

    def visitTypes(self, visitor):
        visitor(self.ret)
        for a in self.args: visitor(a)

    def __str__(self):
        return f"({', '.join(str(a) for a in self.args)}){' const ' if self.const else ' '}-> {self.ret}"

@dataclass
class Template:
    name: QualName

@dataclass
class TemplateInstance:
    template: Template
    args: List[Type]

    def visitTypes(self, visitor):
        for a in self.args: visitor(a)

    #def __str__(self): return f"{self.name}<{', '.join(str(a) for a in self.args)}>"

@dataclass
class Method:
    name: str
    unique_name: str
    type: Type

    def visitTypes(self, visitor):
        visitor(self.type)

@dataclass
class Class:
    name: str # TODO qual?
    properties: Dict[str, Type] = emptyDictField()
    methods: List[Method] = emptyListField()
    static_methods: List[Method] = emptyListField()
    shared_ptr_wrapped: bool = False

    def visitTypes(self, visitor):
        for p in self.properties.values(): visitor(p)
        for m in self.static_methods: visitor(m)
        for m in self.methods: visitor(m)

@dataclass
class Interface(Class):
    shared_ptr_wrapped: bool = True
    pass

@dataclass
class Enumerator:
    name: str
    value: int

@dataclass
class Enum:
    name: str
    is_flag: bool = False
    flag_mask: int = 0xffffffff
    enumerators: List[Enumerator] = emptyListField()

@dataclass
class Field:
    name: str
    type: str
    default: Any = None

@dataclass
class Record:
    name: str
    fields: List[Field] = emptyListField()

@dataclass
class Primitive:
    name: str

@dataclass
class Opaque:
    name: str
