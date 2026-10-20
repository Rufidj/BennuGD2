"""Tabla de mnemónicos de la VM de BennuGD2 (extraída de core/include/pslang.h)."""

MN_1_PARAMS = 0x80
MN_MASK = 0xFF

NAMES = {
    0x00: "END", 0x01: "RETURN", 0x02: "FRAME",
    0x03: "DUP", 0x04: "PUSH", 0x05: "POP",
    0x06: "SWITCH", 0x07: "CASE", 0x08: "CASE_R",
    0x09: "CALL", 0x0A: "SYSCALL", 0x0B: "PROC", 0x0C: "SYSPROC", 0x0D: "CLONE",
    0x0E: "TYPE", 0x0F: "DEBUG",
    0x10: "INDEX", 0x11: "ARRAY", 0x12: "PRIVATE", 0x13: "LOCAL", 0x14: "GLOBAL",
    0x15: "REMOTE", 0x16: "PTR",
    0x17: "GET_PRIV", 0x18: "GET_LOCAL", 0x19: "GET_GLOBAL", 0x1A: "GET_REMOTE",
    0x1B: "JUMP", 0x1C: "JFALSE", 0x1D: "JTRUE", 0x1E: "JTFALSE", 0x1F: "JTTRUE",
    0x20: "JNOCASE",
    0x21: "NEG", 0x22: "NOT",
    0x23: "MUL", 0x24: "DIV", 0x25: "ADD", 0x26: "SUB", 0x27: "MOD",
    0x28: "ROR", 0x29: "ROL", 0x2A: "AND", 0x2B: "OR", 0x2C: "XOR",
    0x2D: "EQ", 0x2E: "NE", 0x2F: "GT", 0x30: "LT", 0x31: "GTE", 0x32: "LTE",
    0x33: "POSTINC", 0x34: "POSTDEC", 0x35: "INC", 0x36: "DEC",
    0x37: "LET", 0x38: "VARADD", 0x39: "VARSUB", 0x3A: "VARMUL", 0x3B: "VARDIV",
    0x3C: "VARMOD", 0x3D: "VARXOR", 0x3E: "VARAND", 0x3F: "VAROR",
    0x40: "VARROR", 0x41: "VARROL",
    0x42: "SUBSTR", 0x43: "STRI2CHR", 0x44: "INT2STR", 0x45: "DOUBLE2STR",
    0x46: "FLOAT2STR", 0x47: "CHR2STR",
    0x48: "LETNP",
    0x49: "BNOT", 0x4A: "BAND", 0x4B: "BOR", 0x4C: "BXOR",
    0x50: "INT2FLOAT", 0x51: "FLOAT2INT", 0x52: "FLOAT2DOUBLE", 0x53: "DOUBLE2FLOAT",
    0x54: "INT2DOUBLE", 0x55: "DOUBLE2INT", 0x56: "STR2CHARNUL",
    0x60: "A2STR", 0x61: "STR2A", 0x62: "STRACAT", 0x63: "INT2DWORD",
    0x64: "INT2WORD", 0x65: "INT2BYTE",
    0x66: "NCALL",
    0x67: "COPY_ARRAY", 0x68: "COPY_ARRAY_REPEAT", 0x69: "COPY_STRUCT",
    0x6A: "EXITHNDLR", 0x6B: "ERRHNDLR", 0x6C: "RESUME", 0x6D: "RESUMEGOTO",
    0x6F: "STR2POINTER",
    0x70: "REPEAT", 0x71: "BREAK", 0x72: "BRFALSE", 0x73: "RETRUE", 0x74: "REFALSE",
    0x75: "POINTER2STR", 0x76: "STR2INT", 0x77: "STR2DOUBLE", 0x78: "STR2FLOAT",
    0x79: "STR2CHR",
    0x7A: "PUBLIC", 0x7B: "GET_PUBLIC", 0x7C: "REMOTE_PUBLIC", 0x7D: "GET_REMOTE_PUBLIC",
    0x7E: "NOP", 0x7F: "SENTENCE",
}

# El bit 0x80 codifica "lleva 1 operando", así que el opcode base es code & 0x7F
BASE = {k: v for k, v in NAMES.items()}

TYPESUFFIX = {0x0000: "", 0x1000: ".dword", 0x2000: ".word", 0x3000: ".byte",
              0x4000: ".string", 0x5000: ".float", 0x6000: ".double"}


def decode(code):
    op = code & 0x7F
    has_param = bool(code & MN_1_PARAMS)
    ty = code & 0x7000
    unsigned = bool(code & 0x8000)
    return BASE.get(op, "UNK_%02X" % op), has_param, ty, unsigned


# Operadores binarios -> símbolo en fuente BennuGD
BINOP = {
    "MUL": "*", "DIV": "/", "ADD": "+", "SUB": "-", "MOD": "%",
    "ROR": ">>", "ROL": "<<", "AND": "&&", "OR": "||", "XOR": "^^",
    "BAND": "&", "BOR": "|", "BXOR": "^",
    "EQ": "==", "NE": "!=", "GT": ">", "LT": "<", "GTE": ">=", "LTE": "<=",
}

ASSIGNOP = {
    "LET": "=", "LETNP": "=", "VARADD": "+=", "VARSUB": "-=", "VARMUL": "*=",
    "VARDIV": "/=", "VARMOD": "%=", "VARXOR": "^=", "VARAND": "&=", "VAROR": "|=",
    "VARROR": ">>=", "VARROL": "<<=",
}

UNOP = {"NEG": "-", "NOT": "!", "BNOT": "~"}

CONVERT = {"POINTER2BOL", "INT2STR", "DOUBLE2STR", "FLOAT2STR", "CHR2STR", "INT2FLOAT", "FLOAT2INT",
           "FLOAT2DOUBLE", "DOUBLE2FLOAT", "INT2DOUBLE", "DOUBLE2INT", "A2STR",
           "STR2A", "INT2DWORD", "INT2WORD", "INT2BYTE", "POINTER2STR", "STR2INT",
           "STR2DOUBLE", "STR2FLOAT", "STR2CHR", "STR2POINTER", "STR2CHARNUL",
           "STRI2CHR"}


# ---------------------------------------------------------------------------
# BennuGD1 (DCB 0x0710). Tabla transcrita de core/include/pslang.h del repo
# oficial de BennuGD1 (SplinterGU/BennuGD), no inferida.
# Difiere de BennuGD2 en dos cosas: faltan los mnemónicos de DOUBLE (y los
# posteriores se corren), y los flags de tipo ocupan el nibble 0xF00 en vez
# de 0xF000.
NAMES_V1 = {
    0x00: 'END',
    0x01: 'RETURN',
    0x02: 'FRAME',
    0x03: 'DUP',
    0x04: 'PUSH',
    0x05: 'POP',
    0x06: 'SWITCH',
    0x07: 'CASE',
    0x08: 'CASE_R',
    0x09: 'CALL',
    0x0A: 'SYSCALL',
    0x0B: 'PROC',
    0x0C: 'SYSPROC',
    0x0D: 'CLONE',
    0x0E: 'TYPE',
    0x0F: 'DEBUG',
    0x10: 'INDEX',
    0x11: 'ARRAY',
    0x12: 'PRIVATE',
    0x13: 'LOCAL',
    0x14: 'GLOBAL',
    0x15: 'REMOTE',
    0x16: 'PTR',
    0x17: 'GET_PRIV',
    0x18: 'GET_LOCAL',
    0x19: 'GET_GLOBAL',
    0x1A: 'GET_REMOTE',
    0x1B: 'JUMP',
    0x1C: 'JFALSE',
    0x1D: 'JTRUE',
    0x1E: 'JTFALSE',
    0x1F: 'JTTRUE',
    0x20: 'JNOCASE',
    0x21: 'NEG',
    0x22: 'NOT',
    0x23: 'MUL',
    0x24: 'DIV',
    0x25: 'ADD',
    0x26: 'SUB',
    0x27: 'MOD',
    0x28: 'ROR',
    0x29: 'ROL',
    0x2A: 'AND',
    0x2B: 'OR',
    0x2C: 'XOR',
    0x2D: 'EQ',
    0x2E: 'NE',
    0x2F: 'GT',
    0x30: 'LT',
    0x31: 'GTE',
    0x32: 'LTE',
    0x33: 'POSTINC',
    0x34: 'POSTDEC',
    0x35: 'INC',
    0x36: 'DEC',
    0x37: 'LET',
    0x38: 'VARADD',
    0x39: 'VARSUB',
    0x3A: 'VARMUL',
    0x3B: 'VARDIV',
    0x3C: 'VARMOD',
    0x3D: 'VARXOR',
    0x3E: 'VARAND',
    0x3F: 'VAROR',
    0x40: 'VARROR',
    0x41: 'VARROL',
    0x42: 'SUBSTR',
    0x43: 'STRI2CHR',
    0x44: 'INT2STR',
    0x45: 'FLOAT2STR',
    0x46: 'CHR2STR',
    0x47: 'LETNP',
    0x48: 'BNOT',
    0x49: 'BAND',
    0x4A: 'BOR',
    0x4B: 'BXOR',
    0x50: 'INT2FLOAT',
    0x51: 'FLOAT2INT',
    0x60: 'A2STR',
    0x61: 'STR2A',
    0x62: 'STRACAT',
    0x63: 'INT2WORD',
    0x64: 'INT2BYTE',
    0x65: 'NCALL',
    0x6A: 'EXITHNDLR',
    0x6B: 'ERRHNDLR',
    0x6C: 'RESUME',
    0x6D: 'RESUMEGOTO',
    0x70: 'REPEAT',
    0x71: 'BREAK',
    0x72: 'BRFALSE',
    0x73: 'RETRUE',
    0x74: 'REFALSE',
    0x75: 'POINTER2STR',
    0x77: 'STR2INT',
    0x78: 'STR2FLOAT',
    0x79: 'STR2CHR',
    0x7A: 'PUBLIC',
    0x7B: 'GET_PUBLIC',
    0x7C: 'REMOTE_PUBLIC',
    0x7D: 'GET_REMOTE_PUBLIC',
    0x7E: 'NOP',
    0x7F: 'SENTENCE',
}

# Flags de tipo de BennuGD1: MN_TYPEOF(code) == code & 0xF00
TYPEMASK_V1 = 0x0F00
TYPESUFFIX_V1 = {0x000: "", 0x100: ".word", 0x200: ".byte",
                 0x300: ".string", 0x400: ".float"}
T_STRING_V1, T_FLOAT_V1, T_DOUBLE_V1 = 0x300, 0x400, None
UNSIGNED_V1 = 0x800

# Flags de tipo de BennuGD2: MN_TYPEOF(code) == code & 0xF000
TYPEMASK = 0xF000
T_STRING, T_FLOAT, T_DOUBLE = 0x4000, 0x5000, 0x6000
UNSIGNED = 0x8000


def decode_v1(code):
    op = code & 0x7F
    return (NAMES_V1.get(op, "UNK_%02X" % op), bool(code & MN_1_PARAMS),
            code & TYPEMASK_V1, bool(code & UNSIGNED_V1))


# ---- Fenix (el antecesor de BennuGD) ----
# Extraida de Fenix/include/pslang.h. Misma codificacion de tipo que
# BennuGD1 (0x0F00 y 0x800), pero los CODIGOS estan desplazados: BennuGD
# metio JTFALSE/JTTRUE (el cortocircuito de && y ||) en 0x1E-0x1F y corrio
# todo lo de detras. Fenix llama CHRSTR a lo que BennuGD1 llama STRI2CHR;
# se usa el nombre de BennuGD para no duplicar el manejo.
NAMES_V0 = {
    0x00: "END", 0x01: "RETURN", 0x02: "FRAME", 0x03: "DUP",
    0x04: "PUSH", 0x05: "POP", 0x06: "SWITCH", 0x07: "CASE",
    0x08: "CASE_R", 0x09: "CALL", 0x0A: "SYSCALL", 0x0B: "PROC",
    0x0C: "SYSPROC", 0x0D: "CLONE", 0x0E: "TYPE", 0x0F: "DEBUG",
    0x10: "INDEX", 0x11: "ARRAY", 0x12: "PRIVATE", 0x13: "LOCAL",
    0x14: "GLOBAL", 0x15: "REMOTE", 0x16: "PTR", 0x17: "GET_PRIV",
    0x18: "GET_LOCAL", 0x19: "GET_GLOBAL", 0x1A: "GET_REMOTE", 0x1B: "JUMP",
    0x1C: "JFALSE", 0x1D: "JTRUE", 0x1E: "JNOCASE", 0x1F: "NEG",
    0x20: "NOT", 0x21: "MUL", 0x22: "DIV", 0x23: "ADD",
    0x24: "SUB", 0x25: "MOD", 0x26: "ROR", 0x27: "ROL",
    0x28: "AND", 0x29: "OR", 0x2A: "XOR", 0x2B: "EQ",
    0x2C: "NE", 0x2D: "GT", 0x2E: "LT", 0x2F: "GTE",
    0x30: "LTE", 0x31: "POSTINC", 0x32: "POSTDEC", 0x33: "INC",
    0x34: "DEC", 0x35: "LET", 0x36: "VARADD", 0x37: "VARSUB",
    0x38: "VARMUL", 0x39: "VARDIV", 0x3A: "VARMOD", 0x3B: "VARXOR",
    0x3C: "VARAND", 0x3D: "VAROR", 0x3E: "VARROR", 0x3F: "VARROL",
    0x40: "SUBSTR", 0x41: "STRI2CHR", 0x42: "INT2STR", 0x43: "FLOAT2STR",
    0x44: "CHR2STR", 0x45: "LETNP", 0x50: "INT2FLOAT", 0x51: "FLOAT2INT",
    0x60: "A2STR", 0x61: "STR2A", 0x62: "STRACAT", 0x63: "INT2WORD",
    0x64: "INT2BYTE", 0x70: "REPEAT", 0x71: "BREAK", 0x72: "BRFALSE",
    0x73: "RETRUE", 0x74: "REFALSE", 0x75: "POINTER2STR", 0x76: "POINTER2BOL",
    0x77: "STR2INT", 0x78: "STR2FLOAT", 0x79: "STR2CHR", 0x7A: "PUBLIC",
    0x7B: "GET_PUBLIC", 0x7C: "REMOTE_PUBLIC", 0x7D: "GET_REMOTE_PUBLIC", 0x7F: "SENTENCE",
}


def decode_v0(code):
    """Igual que decode_v1 pero con la tabla de Fenix."""
    op = code & 0x7F
    return (NAMES_V0.get(op, "UNK_%02X" % op), bool(code & MN_1_PARAMS),
            code & TYPEMASK_V1, bool(code & UNSIGNED_V1))
