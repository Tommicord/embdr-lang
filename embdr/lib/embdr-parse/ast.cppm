/* Copyright(c) 2026 Tommicord
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the “Software”), to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

module;
#include <cstddef>
#include <cstdint>

export module embdr.lex.ast;
import embdr.cxxstd.memoryMaybe;
import embdr.cxxstd.stringView;
import embdr.lex.token;

namespace embdr::lex
{
  using cxxstd::Maybe;
  using cxxstd::SimpleStringView;

  // "no node" index sentinel. Node ids are uint32_t indices into the arena;
  // NO_NODE is the structural end-of-list marker for first_child /
  // next_sibling links (never a failure signal — failures are Maybe values).
  export inline constexpr uint32_t NO_NODE = 0xFFFFFFFFu;

  // Parser resource ceilings. They are template defaults, not hard limits:
  // a Parser<AstArena<S>> instantiation picks its own budgets (tests use
  // tiny arenas to exercise the ARENA_FULL failure path).
  export inline constexpr size_t PARSE_MAX_NODES = 8192;
  export inline constexpr size_t PARSE_MAX_DEPTH = 128;
  export inline constexpr size_t PARSE_MAX_DIAGNOSTICS = 32;
  export inline constexpr size_t PARSE_MAX_ATTRIBUTES = 8;

  // Flat, kind-tagged AST. Grouping is the .def-file contract relied on by
  // the is_* range helpers: items, then types, then statements, then
  // expressions, then patterns — a new enumerator goes inside its group.
  export enum class NodeKind : uint16_t {
    // Items (translation-unit level and member level).
    TRANSLATION_UNIT,
    MODULE,
    USE_DECL,
    FUNCTION_DECL,
    PARAM_DECL,
    GENERIC_PARAMS,
    STATIC_DECL,
    STRUCT_DECL,
    FIELD_DECL,
    ENUM_DECL,
    VARIANT_DECL,
    MACRO_DECL,
    MACRO_RULE,
    EMBED_DECL,
    ATTRIBUTE,

    // Types.
    TYPE_NAME,
    TYPE_POINTER,
    TYPE_REFERENCE,
    TYPE_ARRAY,
    TYPE_FUNCTION,

    // Statements.
    BLOCK,
    LOCAL_DECL,
    EXPR_STMT,
    IF,
    WHILE,
    LOOP,
    FOR,
    MATCH,
    MATCH_ARM,
    RET,
    BREAK,
    CONTINUE,
    DEFER,
    VOLATILE_BLOCK,
    ASM_BLOCK,
    ASM_INSTR,
    LABEL,

    // Expressions.
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    CHAR_LITERAL,
    BOOL_LITERAL,
    PATH,
    MEMBER_ACCESS,
    CALL,
    INDEX,
    UNARY,
    BINARY,
    ASSIGN,
    CAST_EXPR,
    RANGE,
    TRY_EXPR,
    COALESCE,
    PIPELINE,
    ARRAY_LITERAL,
    STRUCT_LITERAL,
    FIELD_INIT,
    CLOSURE,
    MACRO_CALL,

    // Patterns.
    PATTERN_VARIANT,
    PATTERN_LITERAL,
    PATTERN_RANGE,
    PATTERN_ALT,
    PATTERN_BINDING,
    PATTERN_WILDCARD,
    PATTERN_REGEX,
    PATTERN_REST,
    MACRO_PATTERN,
    MACRO_CAPTURE,
  };

  // Node flags. The bits are per-kind: e.g. on FUNCTION_DECL bit 0 is `pub`
  // and bit 1 is `extern`, on RANGE bit 0 selects `..=` over `..`, on PATH
  // bit 0 records a leading `::`. KIND COMMENTS BELOW DOCUMENT THE MEANING.
  export enum class NodeFlags : uint8_t {
    NONE = 0,
    PUB = 1U << 0U,
    EXTERN = 1U << 1U,
    STATIC_STORAGE = 1U << 2U,
    INTERRUPT = 1U << 3U,
    VOLATILE = 1U << 4U,
    CEXPR = 1U << 5U,
    PACKED = 1U << 6U,
    REFLECT = 1U << 7U,
  };

  export [[nodiscard]] constexpr NodeFlags
  operator| (const NodeFlags lhs, const NodeFlags rhs) noexcept
  {
    return static_cast<NodeFlags> (static_cast<uint8_t> (lhs) | static_cast<uint8_t> (rhs));
  }

  export [[nodiscard]] constexpr bool
  any (const NodeFlags flags) noexcept
  {
    return static_cast<uint8_t> (flags) != 0;
  }

  export [[nodiscard]] constexpr bool
  has_flag (const uint8_t flags, const NodeFlags bit) noexcept
  {
    return (flags & static_cast<uint8_t> (bit)) != 0;
  }

  // RANGE uses the low bits outside NodeFlags for its shape.
  export inline constexpr uint8_t RANGE_INCLUSIVE = 1U << 0U;
  export inline constexpr uint8_t RANGE_HAS_LO = 1U << 1U;
  export inline constexpr uint8_t RANGE_HAS_HI = 1U << 2U;

  // LOCAL_DECL / FOR bookkeeping bits (outside NodeFlags' item meaning).
  export inline constexpr uint8_t LOCAL_NO_TYPE = 1U << 0U; // `let`-style binding
  export inline constexpr uint8_t LOCAL_STATIC = 1U << 1U;
  export inline constexpr uint8_t FOR_CEXPR = 1U << 0U;
  export inline constexpr uint8_t FOR_HAS_INDEX = 1U << 1U;
  export inline constexpr uint8_t CLOSURE_STATIC = 1U << 0U;
  export inline constexpr uint8_t IF_HAS_ELSE = 1U << 0U;
  export inline constexpr uint8_t DTOR_DEFAULT = 1U << 0U;
  export inline constexpr uint8_t MEMBER_CTOR = 1U << 6U;
  export inline constexpr uint8_t MEMBER_DTOR = 1U << 7U;

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const NodeKind kind) noexcept
  {
    switch (kind)
      {
        case NodeKind::TRANSLATION_UNIT:
          return "translation unit";
        case NodeKind::MODULE:
          return "module";
        case NodeKind::USE_DECL:
          return "use declaration";
        case NodeKind::FUNCTION_DECL:
          return "function declaration";
        case NodeKind::PARAM_DECL:
          return "parameter";
        case NodeKind::GENERIC_PARAMS:
          return "generic parameters";
        case NodeKind::STATIC_DECL:
          return "static declaration";
        case NodeKind::STRUCT_DECL:
          return "struct declaration";
        case NodeKind::FIELD_DECL:
          return "field";
        case NodeKind::ENUM_DECL:
          return "enum declaration";
        case NodeKind::VARIANT_DECL:
          return "variant";
        case NodeKind::MACRO_DECL:
          return "macro declaration";
        case NodeKind::MACRO_RULE:
          return "macro rule";
        case NodeKind::EMBED_DECL:
          return "embed declaration";
        case NodeKind::ATTRIBUTE:
          return "attribute";
        case NodeKind::TYPE_NAME:
          return "type name";
        case NodeKind::TYPE_POINTER:
          return "pointer type";
        case NodeKind::TYPE_REFERENCE:
          return "reference type";
        case NodeKind::TYPE_ARRAY:
          return "array type";
        case NodeKind::TYPE_FUNCTION:
          return "function type";
        case NodeKind::BLOCK:
          return "block";
        case NodeKind::LOCAL_DECL:
          return "local declaration";
        case NodeKind::EXPR_STMT:
          return "expression statement";
        case NodeKind::IF:
          return "if";
        case NodeKind::WHILE:
          return "while";
        case NodeKind::LOOP:
          return "loop";
        case NodeKind::FOR:
          return "for";
        case NodeKind::MATCH:
          return "match";
        case NodeKind::MATCH_ARM:
          return "match arm";
        case NodeKind::RET:
          return "ret";
        case NodeKind::BREAK:
          return "break";
        case NodeKind::CONTINUE:
          return "continue";
        case NodeKind::DEFER:
          return "defer";
        case NodeKind::VOLATILE_BLOCK:
          return "volatile block";
        case NodeKind::ASM_BLOCK:
          return "asm block";
        case NodeKind::ASM_INSTR:
          return "asm instruction";
        case NodeKind::LABEL:
          return "label";
        case NodeKind::INT_LITERAL:
          return "integer literal";
        case NodeKind::FLOAT_LITERAL:
          return "float literal";
        case NodeKind::STRING_LITERAL:
          return "string literal";
        case NodeKind::CHAR_LITERAL:
          return "char literal";
        case NodeKind::BOOL_LITERAL:
          return "bool literal";
        case NodeKind::PATH:
          return "path";
        case NodeKind::MEMBER_ACCESS:
          return "member access";
        case NodeKind::CALL:
          return "call";
        case NodeKind::INDEX:
          return "index";
        case NodeKind::UNARY:
          return "unary expression";
        case NodeKind::BINARY:
          return "binary expression";
        case NodeKind::ASSIGN:
          return "assignment";
        case NodeKind::CAST_EXPR:
          return "cast";
        case NodeKind::RANGE:
          return "range";
        case NodeKind::TRY_EXPR:
          return "try expression";
        case NodeKind::COALESCE:
          return "coalesce";
        case NodeKind::PIPELINE:
          return "pipeline";
        case NodeKind::ARRAY_LITERAL:
          return "array literal";
        case NodeKind::STRUCT_LITERAL:
          return "struct literal";
        case NodeKind::FIELD_INIT:
          return "field initializer";
        case NodeKind::CLOSURE:
          return "closure";
        case NodeKind::MACRO_CALL:
          return "macro call";
        case NodeKind::PATTERN_VARIANT:
          return "variant pattern";
        case NodeKind::PATTERN_LITERAL:
          return "literal pattern";
        case NodeKind::PATTERN_RANGE:
          return "range pattern";
        case NodeKind::PATTERN_ALT:
          return "alternative pattern";
        case NodeKind::PATTERN_BINDING:
          return "binding pattern";
        case NodeKind::PATTERN_WILDCARD:
          return "wildcard pattern";
        case NodeKind::PATTERN_REGEX:
          return "regex pattern";
        case NodeKind::PATTERN_REST:
          return "rest pattern";
        case NodeKind::MACRO_PATTERN:
          return "macro pattern";
        case NodeKind::MACRO_CAPTURE:
          return "macro capture";
      }
    return "unknown node";
  }

  export [[nodiscard]] constexpr bool
  is_item_kind (const NodeKind kind) noexcept
  {
    return kind >= NodeKind::TRANSLATION_UNIT && kind <= NodeKind::ATTRIBUTE;
  }

  export [[nodiscard]] constexpr bool
  is_type_kind (const NodeKind kind) noexcept
  {
    return kind >= NodeKind::TYPE_NAME && kind <= NodeKind::TYPE_FUNCTION;
  }

  export [[nodiscard]] constexpr bool
  is_stmt_kind (const NodeKind kind) noexcept
  {
    return kind >= NodeKind::BLOCK && kind <= NodeKind::LABEL;
  }

  export [[nodiscard]] constexpr bool
  is_expr_kind (const NodeKind kind) noexcept
  {
    return kind >= NodeKind::INT_LITERAL && kind <= NodeKind::MACRO_CALL;
  }

  export [[nodiscard]] constexpr bool
  is_pattern_kind (const NodeKind kind) noexcept
  {
    return kind >= NodeKind::PATTERN_VARIANT && kind <= NodeKind::MACRO_CAPTURE;
  }

  // The parser's diagnostic value. Carries enough data to format a
  // clang-style "expected X, found Y" note without re-lexing.
  export struct ParseError
  {
    enum class Kind : uint8_t
    {
      EXPECTED_TOKEN, // expected a specific punctuator/keyword
      EXPECTED_IDENTIFIER,
      EXPECTED_EXPRESSION,
      EXPECTED_TYPE,
      EXPECTED_ITEM,
      EXPECTED_PATTERN,
      UNEXPECTED_EOF,
      UNEXPECTED_TOKEN,
      NESTING_TOO_DEEP,
      ARENA_FULL,
      TOO_MANY_ERRORS,
      NON_ASSOC_CHAIN, // a < b < c — non-associative operator chained
      LEX_ERROR, // forwarded from the lexer
    };

    Kind kind = Kind::EXPECTED_TOKEN;
    uint32_t offset = 0;
    TokenKind expected = TokenKind::END_OF_FILE;
    TokenKind found = TokenKind::END_OF_FILE;
  };

  export [[nodiscard]] constexpr SimpleStringView
  as_str (const ParseError::Kind kind) noexcept
  {
    switch (kind)
      {
        case ParseError::Kind::EXPECTED_TOKEN:
          return "unexpected token";
        case ParseError::Kind::EXPECTED_IDENTIFIER:
          return "expected an identifier";
        case ParseError::Kind::EXPECTED_EXPRESSION:
          return "expected an expression";
        case ParseError::Kind::EXPECTED_TYPE:
          return "expected a type";
        case ParseError::Kind::EXPECTED_ITEM:
          return "expected an item";
        case ParseError::Kind::EXPECTED_PATTERN:
          return "expected a pattern";
        case ParseError::Kind::UNEXPECTED_EOF:
          return "unexpected end of file";
        case ParseError::Kind::UNEXPECTED_TOKEN:
          return "unexpected token";
        case ParseError::Kind::NESTING_TOO_DEEP:
          return "syntax nested too deeply";
        case ParseError::Kind::ARENA_FULL:
          return "AST arena is full";
        case ParseError::Kind::TOO_MANY_ERRORS:
          return "too many errors, giving up";
        case ParseError::Kind::NON_ASSOC_CHAIN:
          return "non-associative operator cannot be chained";
        case ParseError::Kind::LEX_ERROR:
          return "invalid token";
      }
    return "unknown parse error";
  }

  // One AST node. Children are a singly linked list threaded through the
  // arena (first_child -> next_sibling); parent keeps last_child for O(1)
  // appends. `text` and literal payloads point into the source buffer, which
  // must outlive the arena (documented lifetime for parse results).
  export struct AstNode
  {
    NodeKind kind = NodeKind::TRANSLATION_UNIT;
    TokenKind op = TokenKind::END_OF_FILE; // operator / associated token
    uint8_t flags = 0; // per-kind bits, see NodeFlags and *_bits above
    uint32_t offset = 0; // byte offset of the construct in the source
    uint32_t first_child = NO_NODE;
    uint32_t last_child = NO_NODE;
    uint32_t next_sibling = NO_NODE;
    SimpleStringView text{}; // name / literal lexeme (source view)
    uint64_t value = 0; // folded integer value / char scalar / misc
  };

  // Fixed-capacity node arena. make() is the only way to create a node, so
  // a full arena surfaces as an ARENA_FULL error value instead of an
  // allocation or a truncated tree. rollback() truncates to a saved count —
  // the speculative-parse undo (nodes are append-only, so it is O(1)).
  export template <size_t Capacity = PARSE_MAX_NODES>
  class AstArena
  {
    static_assert (Capacity > 0, "embdr::lex::AstArena capacity must be positive");

  public:
    static constexpr size_t capacity_value = Capacity;

    [[nodiscard]] constexpr Maybe<uint32_t, ParseError>
    make (const NodeKind kind, const uint32_t offset) noexcept
    {
      if (_M_count >= Capacity)
        {
          return ParseError{ParseError::Kind::ARENA_FULL, offset, TokenKind::END_OF_FILE, TokenKind::END_OF_FILE};
        }
      const uint32_t id = _M_count;
      AstNode& node = _M_nodes[_M_count];
      node.kind = kind;
      node.op = TokenKind::END_OF_FILE;
      node.flags = 0;
      node.offset = offset;
      node.first_child = NO_NODE;
      node.last_child = NO_NODE;
      node.next_sibling = NO_NODE;
      node.text = SimpleStringView{};
      node.value = 0;
      ++_M_count;
      return id;
    }

    [[nodiscard]] constexpr AstNode&
    operator[] (const uint32_t id) noexcept
    {
      return _M_nodes[id];
    }

    [[nodiscard]] constexpr const AstNode&
    operator[] (const uint32_t id) const noexcept
    {
      return _M_nodes[id];
    }

    [[nodiscard]] constexpr uint32_t
    count () const noexcept
    {
      return _M_count;
    }

    // Appends `child` to `parent`'s child list. Precondition: both ids were
    // produced by make() — violating it terminates (noexcept discipline).
    constexpr void
    append (const uint32_t parent, const uint32_t child) noexcept
    {
      AstNode& p = _M_nodes[parent];
      AstNode& c = _M_nodes[child];
      if (p.first_child == NO_NODE)
        {
          p.first_child = child;
        }
      else
        {
          _M_nodes[p.last_child].next_sibling = child;
        }
      p.last_child = child;
      c.next_sibling = NO_NODE;
    }

    // Undoes every node allocated after `saved` (speculative parse rollback).
    constexpr void
    rollback (const uint32_t saved) noexcept
    {
      if (saved < _M_count)
        {
          _M_count = saved;
        }
    }

  private:
    AstNode _M_nodes[Capacity]{};
    uint32_t _M_count = 0;
  };
} // namespace embdr::lex
