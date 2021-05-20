from abc import abstractmethod
import enum
from typing import Any, Dict, Generic, List, NamedTuple, Text, TypeVar, Union
from data_model import *

T = TypeVar('T')


class Cursor(Generic[T]):
    def __init__(self, data):
        self.data = data
        self.pos = 0

    @property
    def atEnd(self):
        assert self.pos <= len(self.data)
        return self.pos == len(self.data)

    def __getitem__(self, i) -> T:
        try:
            return self.data[self.pos + i]
        except IndexError:
            return self.missing()

    @property
    def cur(self):
        return self[0]

    @property
    def next(self):
        return self[1]

    def advance(self):
        self.pos += 1

    @abstractmethod
    def missing() -> T: pass

def _charRangeSet(a, b):
    assert b >= a
    return {chr(x) for x in range(ord(a), ord(b))}

_digits = _charRangeSet('0', '9')
_identStartChars = {'_'} | _charRangeSet('a', 'z') | _charRangeSet('A', 'Z')
_identChars = _identStartChars | _digits

_numChars = _digits | {'.'}

class StrCursor(Cursor[str]):
    data: str

    def skipPastSpaces(self):
        while self.cur == ' ':
            self.advance()

    def consumeWhileInSet(self, set):
        out = []
        while self.cur in set:
            out.append(self.cur)
            self.advance()
        return ''.join(out)

    def consumeIdent(self): return self.consumeWhileInSet(_identChars)
    def consumeNum(self): return self.consumeWhileInSet(_numChars)

    def consumeOperator(self, ops):
        for o in ops:
            if self.data.startswith(o, self.pos):
                self.pos += len(o)
                return OP(o)
        assert False

    @staticmethod
    def missing(): return '\0'

    def __str__(self):
        return self.data + '\n' + (' '*self.pos) + '^'

class TokKind(enum.Enum):
    END = enum.auto()
    NUM = enum.auto()
    STR = enum.auto()
    CHAR = enum.auto()
    IDENT = enum.auto()
    OP = enum.auto()

    def __str__(self):
        return self._name_
    def __call__(self, raw):
        return Token(self, raw)

END = TokKind.END
NUM = TokKind.NUM
STR = TokKind.STR
CHAR = TokKind.CHAR
IDENT = TokKind.IDENT
OP = TokKind.OP

class Token(NamedTuple):
    kind: TokKind
    raw: str

    def __str__(self):
        return f'{self.kind}("{self.raw}")'

    __repr__ = __str__


def tokenize(c: Union[StrCursor, str]):
    if (isinstance(c, str)):
        c = StrCursor(c)
    try:
        result = []
        while not c.atEnd:
            cur = c.cur
            if cur == ' ':
                c.skipPastSpaces()
            elif cur in _identStartChars:
                result.append(IDENT(c.consumeIdent()))
                # Assuming no prefixed strings.
                assert c.cur not in {'"', "'"}
            elif cur in _digits:
                # only support base 10 numbers
                assert cur != '0'
                result.append(NUM(c.consumeNum()))
            elif cur in {'{', '}', '[', ']', '(', ')', '?', ',', '~', ';'}:
              result.append(OP(cur))
              c.advance()
            # Not supporting alt tokens
            elif cur == '!': result.append(c.consumeOperator(("!=", "!")))
            elif cur == '#': result.append(c.consumeOperator(("##", "#")))
            elif cur == '%': result.append(c.consumeOperator(("%=", "%"))) # %> %: %:%:
            elif cur == '&': result.append(c.consumeOperator(("&&", "&=", "&")))
            elif cur == '*': result.append(c.consumeOperator(("*=", "*")))
            elif cur == '+': result.append(c.consumeOperator(("++", "+=", "+")))
            elif cur == '-': result.append(c.consumeOperator(("->*", "--", "-=", "->", "-")))
            elif cur == '/': result.append(c.consumeOperator(("/=", "/")))
            elif cur == ':': result.append(c.consumeOperator(("::", ":"))) # :>
            elif cur == '<': result.append(c.consumeOperator(("<<=", "<=>", "<=", "<<", "<"))) # <: <%
            elif cur == '>': result.append(c.consumeOperator((">>=", ">>", ">=", ">")))
            elif cur == '=': result.append(c.consumeOperator(("==", "=")))
            elif cur == '^': result.append(c.consumeOperator(("^=", "^")))
            elif cur == '|': result.append(c.consumeOperator(("|=", "||", "|")))
            elif cur == '.':
                if c.next in _digits:
                    result.append(NUM(c.consumeNum()))
                else:
                    result.append(c.consumeOperator(("...", ".*", ".")))
            elif cur == "'":
                start = c.pos
                if c.next == '\\':
                    c.pos += 4
                else:
                    c.pos += 3
                result.append(CHAR(c.data[start+1:c.pos-1]))
            elif cur == '"':
                start = c.pos
                while True:
                    c.advance()
                    assert not c.atEnd
                    if c.cur == '"': break
                    elif c.cur == '\\':
                      c.advance() # also consume escaped char
                assert c.cur == '"'
                c.advance()
                result.append(STR(c.data[start+1:c.pos-1]))
            else:
                assert False
        return result
    except:
        print(c)
        raise


class TokCursor(Cursor[Token]):
    data: List[Token]

    def __init__(self, data):
        if isinstance(data, str):
            data = tokenize(data)
        Cursor.__init__(self, data)

    def try_consume(self, kind):
        if (isinstance(kind, Token)):
            if self.cur != kind: return None
        else:
            assert isinstance(kind, TokKind)
            if self.cur.kind != kind: return None

        self.advance()
        return self[-1]

    def consume(self, kind):
        if (isinstance(kind, Token)):
            assert self.cur == kind
        else:
            assert isinstance(kind, TokKind)
            assert self.cur.kind == kind

        self.advance()
        return self[-1]

    @staticmethod
    def missing(): return END('')

    def __str__(self):
        lines = []
        for i, tok in enumerate(self.data):
            if i == self.pos:
                lines.append(f"> {tok}")
            else:
                lines.append(f"  {tok}")
        if self.pos >= len(self.data):
            lines.append('>')
        return '\n'.join(lines)


def parseQualName(toks):
    if not isinstance(toks, TokCursor):
        toks = TokCursor(toks)

    names = [toks.consume(IDENT).raw]
    while toks.try_consume(OP('::')):
        names.append(toks.consume(IDENT).raw)
    return QualName(names)


def parseType(toks: TokCursor, syms):
    if toks.cur == OP('('):
        return parseSig(toks, syms)

    is_const = toks.try_consume(IDENT('const')) is not None

    type = parseQualName(toks)
    if toks.try_consume(OP('<')):
        tempArgs = []
        while True:
            tempArgs.append(parseType(toks, syms))
            if toks.try_consume(OP('>')):
                break
            assert toks.consume(OP(','))

        type = TemplateInstance(syms.templates[type], tempArgs)
    else:
        type = syms.types[type]

    if is_const:
        type = Const(type)

    while True:
        if toks.try_consume(IDENT('const')):
            type = Const(type)
        elif toks.try_consume(OP('*')):
            type = Pointer(type)
        elif toks.try_consume(OP('&')):
            type = Ref(type)
        elif toks.try_consume(OP('&&')):
            type = RRef(type)
        else:
            break
    return type


def parseSig(toks: TokCursor, syms):
    args = []
    toks.consume(OP('('))
    if not toks.try_consume(OP(')')):
        while True:
            name = toks.consume(IDENT).raw
            toks.consume(OP(':'))
            type = parseType(toks, syms)
            args.append(Arg(name, type))

            if toks.try_consume(OP(')')):
                break
            assert toks.consume(OP(','))

    is_const = toks.try_consume(IDENT('const')) is not None
    is_noexcept = toks.try_consume(IDENT('noexcept')) is not None

    if toks.try_consume(OP('->')):
        ret = parseType(toks, syms)
    else:
        ret = syms.types[parseQualName('void')]

    return FuncSig(ret, args, is_const, is_noexcept)

def parse(toks: Union[TokCursor, str, List[Token]], syms):
    if (not isinstance(toks, TokCursor)):
        if (isinstance(toks, str)):
            toks = TokCursor(tokenize(toks))
        else:
            toks = TokCursor(toks)

    try:
        return parseType(toks, syms)
    except:
        print(toks)
        raise


class SymbolTable:
    def __init__(self):
        self.templates = {}
        self.types = {}
        self.classes = []
        self.enums = []
        self.records = []

    def addType(self, name, t):
        if not isinstance(name, QualName): name = parseQualName(name)
        assert name not in self.types
        self.types[name] = t

    def addTemplate(self, name, t):
        if not isinstance(name, QualName): name = parseQualName(name)
        assert name not in self.templates
        self.templates[name] = t
