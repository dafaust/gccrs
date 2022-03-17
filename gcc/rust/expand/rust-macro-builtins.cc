// Copyright (C) 2020-2022 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "rust-macro-builtins.h"
#include "rust-diagnostics.h"
#include "rust-expr.h"
#include "rust-session-manager.h"
#include "rust-macro-invoc-lexer.h"
#include "rust-lex.h"
#include "rust-parse.h"

namespace Rust {
namespace {
std::unique_ptr<AST::Expr>
make_string (Location locus, std::string value)
{
  return std::unique_ptr<AST::Expr> (
    new AST::LiteralExpr (value, AST::Literal::STRING,
			  PrimitiveCoreType::CORETYPE_STR, {}, locus));
}

std::unique_ptr <AST::LiteralExpr>
parse_single_string_literal (AST::DelimTokenTree &invoc_token_tree)
{
  MacroInvocLexer lex (invoc_token_tree.to_token_stream());
  Parser<MacroInvocLexer> parser (std::move (lex));

  auto last_token_id = TokenId::RIGHT_CURLY;
  switch (invoc_token_tree.get_delim_type ())
    {
    case AST::DelimType::PARENS:
      last_token_id = TokenId::RIGHT_PAREN;
      rust_assert (parser.skip_token (LEFT_PAREN));
      break;

    case AST::DelimType::CURLY:
      rust_assert (parser.skip_token (LEFT_CURLY));
      break;

    case AST::DelimType::SQUARE:
      last_token_id = TokenId::RIGHT_SQUARE;
      rust_assert (parser.skip_token (LEFT_SQUARE));
      break;
    }

  rust_assert (parser.peek_current_token ()->get_id () == STRING_LITERAL);
  std::unique_ptr <AST::LiteralExpr> lit_expr = parser.parse_literal_expr ();

  rust_assert (parser.skip_token (last_token_id));

  return lit_expr;
}

std::vector<char>
load_file_bytes (const char * filename)
{
  RAIIFile file_wrap (filename);
  if (file_wrap.get_raw () == nullptr)
    rust_fatal_error (Location (), "cannot open filename %s: %m", filename);

  FILE *f = file_wrap.get_raw ();
  fseek (f, 0L, SEEK_END);
  long fsize = ftell (f);
  fseek (f, 0L, SEEK_SET);

  std::vector <char> buf(fsize);

  fread (&buf[0], 1, fsize, f);
  int err;
  if ((err = ferror (f)) != 0)
    rust_fatal_error (Location (), "error reading file %s: %d: %m", filename, err);

  return buf;
}

} // namespace

AST::ASTFragment
MacroBuiltin::assert (Location invoc_locus, AST::MacroInvocData &invoc)
{
  rust_debug ("assert!() called");

  return AST::ASTFragment::create_empty ();
}

AST::ASTFragment
MacroBuiltin::file (Location invoc_locus, AST::MacroInvocData &invoc)
{
  auto current_file
    = Session::get_instance ().linemap->location_file (invoc_locus);
  auto file_str = AST::SingleASTNode (make_string (invoc_locus, current_file));

  return AST::ASTFragment ({file_str});
}
AST::ASTFragment
MacroBuiltin::column (Location invoc_locus, AST::MacroInvocData &invoc)
{
  auto current_column
    = Session::get_instance ().linemap->location_to_column (invoc_locus);
  // auto column_no
  //   = AST::SingleASTNode (make_string (invoc_locus, current_column));

  auto column_no = AST::SingleASTNode (std::unique_ptr<AST::Expr> (
    new AST::LiteralExpr (std::to_string (current_column), AST::Literal::INT,
			  PrimitiveCoreType::CORETYPE_U32, {}, invoc_locus)));

  return AST::ASTFragment ({column_no});
}


/* Expand builtin macro include!("filename"), which includes the contents of
   the given file parsed as an expression.  */

  #if 0
AST::ASTFragment
MacroBuiltin::include (Location invoc_locus, AST::MacroInvocData &invoc)
{
  // TODO
}
  #endif

/* Expand builtin macro include_bytes!("filename"), which includes the contents
   of the given file as reference to a byte array. Yields an expression of type
   &'static [u8; N].  */

AST::ASTFragment
MacroBuiltin::include_bytes (Location invoc_locus, AST::MacroInvocData &invoc)
{
  // "filename" lives as a Literal token in the MacroInvocData's
  // DelimTokenTree

  auto lit = parse_single_string_literal (invoc.get_delim_tok_tree ());
  const char * filename = lit->as_string ().c_str ();

  std::vector<char> bytes = load_file_bytes (filename);

  /* Is there a more efficient way to do this?  */
  std::vector<std::unique_ptr<AST::Expr>> elts;
  for (char b : bytes)
    {
      elts.emplace_back
	(new AST::LiteralExpr (std::string ((const char *) &b),
			       AST::Literal::BYTE,
			       PrimitiveCoreType::CORETYPE_U8,
			       {} /* outer_attrs */,
			       invoc_locus));
    }

  auto elems = std::unique_ptr<AST::ArrayElems>
    (new AST::ArrayElemsValues (std::move (elts), invoc_locus));

  auto array = std::unique_ptr<AST::Expr>
    (new AST::ArrayExpr (std::move (elems), {}, {}, invoc_locus));

  auto node = AST::SingleASTNode (std::move (array));

  return AST::ASTFragment ({node});
}

/* Expand builtin macro include_str!("filename"), which includes the contents
   of the given file as a string. The file must be UTF-8 encoded. Yields an
   expression of type &'static str.  */

AST::ASTFragment
MacroBuiltin::include_str (Location invoc_locus, AST::MacroInvocData &invoc)
{

  auto lit = parse_single_string_literal (invoc.get_delim_tok_tree ());
  const char * filename = lit->as_string ().c_str ();

  std::vector<char> bytes = load_file_bytes (filename);

  std::string str(&bytes[0], bytes.size ());

  // TODO: Enforce that the file contents are valid UTF-8?

  auto node = AST::SingleASTNode (make_string (invoc_locus, str));

  return AST::ASTFragment ({node});
}

} // namespace Rust
