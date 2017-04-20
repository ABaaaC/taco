#include "taco/parser/parser.h"

#include "taco/parser/lexer.h"
#include "taco/tensor_base.h"
#include "taco/format.h"
#include "taco/expr.h"
#include "taco/operator.h"
#include "taco/expr_nodes/expr_rewriter.h"
#include "taco/util/collections.h"

using namespace std;

namespace taco {
namespace parser {

struct Parser::Content {
  /// Tensor formats
  map<string,Format> formats;

  /// Tensor dimensions
  map<string,std::vector<int>> dimensionSizes;
  map<Var, int>                indexVarSizes;

  int dimensionDefault;

  /// Track which dimensions has default values, so that we can change them
  /// to values inferred from other tensors (read from files).
  set<pair<TensorBase,size_t>> defaultDimension;

  Lexer lexer;
  Token currentToken;
  bool parsingLhs = false;

  map<string,Var> indexVars;

  TensorBase             resultTensor;
  map<string,TensorBase> tensors;
};

Parser::Parser(string expression, const map<string,Format>& formats,
               const map<string,std::vector<int>>& dimensionSizes,
               const std::map<std::string,TensorBase>& tensors,
               int dimensionDefault)
    : content(new Parser::Content) {
  content->lexer = Lexer(expression);
  content->formats = formats;
  content->dimensionSizes = dimensionSizes;
  content->dimensionDefault = dimensionDefault;
  content->tensors = tensors;
  nextToken();
}

void Parser::parse() {
  content->resultTensor = parseAssign();
}

const TensorBase& Parser::getResultTensor() const {
  return content->resultTensor;
}

TensorBase Parser::parseAssign() {
  content->parsingLhs = true;
  Read lhs = parseAccess();
  content->parsingLhs = false;
  consume(Token::eq);
  Expr rhs = parseExpr();

  // Collect all index var dimension sizes
  struct Visitor : expr_nodes::ExprVisitor {
    set<pair<TensorBase,size_t>> defaultDimension;
    map<taco::Var, int>* indexVarSizes;

    void visit(const expr_nodes::ReadNode* op) {
      for (size_t i = 0; i < op->indexVars.size(); i++) {
        Var indexVar = op->indexVars[i];
        if (!util::contains(defaultDimension, {op->tensor,i})) {
          int dimension = op->tensor.getDimensions()[i];
          if (util::contains(*indexVarSizes, indexVar)) {
            taco_uassert(indexVarSizes->at(indexVar) == dimension)
                << "Incompatible dimensions";
          }
          else {
            indexVarSizes->insert({indexVar, dimension});
          }
        }
      }
    }
  };
  Visitor visitor;
  visitor.indexVarSizes = &content->indexVarSizes;
  visitor.defaultDimension = content->defaultDimension;
  rhs.accept(&visitor);

  // Rewrite expression to new index sizes
  struct Rewriter : expr_nodes::ExprRewriter {
    map<taco::Var, int>* indexVarSizes;
    map<string,TensorBase> tensors;

    void visit(const expr_nodes::ReadNode* op) {
      bool dimensionChanged = false;
      vector<int> dimensions = op->tensor.getDimensions();
      taco_iassert(dimensions.size() == op->indexVars.size());
      for (size_t i=0; i < dimensions.size(); i++) {
        Var indexVar = op->indexVars[i];
        if (util::contains(*indexVarSizes, indexVar)) {
          int dimSize = indexVarSizes->at(indexVar);
          if (dimSize != dimensions[i]) {
            dimensions[i] = dimSize;
            dimensionChanged = true;
          }
        }
      }
      if (dimensionChanged) {
        TensorBase tensor;
        if (util::contains(tensors, op->tensor.getName())) {
          tensor = tensors.at(op->tensor.getName());
        }
        else {
          tensor = TensorBase(op->tensor.getName(),
                              op->tensor.getComponentType(), dimensions,
                              op->tensor.getFormat(),op->tensor.getAllocSize());
          tensors.insert({tensor.getName(), tensor});
        }
        expr = new expr_nodes::ReadNode(tensor, op->indexVars);
      }
      else {
        expr = op;
      }
    }
  };
  Rewriter rewriter;
  rewriter.indexVarSizes = visitor.indexVarSizes;
  rhs = rewriter.rewrite(rhs);

  Expr rewrittenLhs = rewriter.rewrite(lhs);

  for (auto& tensor : rewriter.tensors) {
    content->tensors.at(tensor.first) = tensor.second;
  }
  content->resultTensor = content->tensors.at(lhs.getTensor().getName());
  content->resultTensor.setExpr(rhs);
  content->resultTensor.setIndexVars(lhs.getIndexVars());
  return content->resultTensor;
}

Expr Parser::parseExpr() {
  Expr expr = parseTerm();
  while (content->currentToken == Token::add ||
         content->currentToken == Token::sub) {
    switch (content->currentToken) {
      case Token::add:
        consume(Token::add);
        expr = expr + parseTerm();
        break;
      case Token::sub:
        consume(Token::sub);
        expr = expr - parseTerm();
        break;
      default:
        taco_unreachable;
    }
  }
  return expr;
}

Expr Parser::parseTerm() {
  Expr term = parseFactor();
  while (content->currentToken == Token::mul) {
    switch (content->currentToken) {
      case Token::mul:
        consume(Token::mul);
        term = term * parseFactor();
        break;
      default:
        taco_unreachable;
    }
  }
  return term;
}

Expr Parser::parseFactor() {
  switch (content->currentToken) {
    case Token::lparen: {
      consume(Token::lparen);
      Expr factor = parseExpr();
      consume(Token::rparen);
      return factor;
    }
    case Token::sub:
      consume(Token::sub);
      return Neg(parseFactor());
    default:
      break;
  }
  return parseFinal();
}

Expr Parser::parseFinal() {
  if(content->currentToken == Token::scalar) {
    string value=content->lexer.getIdentifier();
    consume(Token::scalar);
    return Expr(atof(value.c_str()));
  }
  else
    return parseAccess();
}

Read Parser::parseAccess() {
  if(content->currentToken != Token::identifier) {
    throw ParseError("Expected tensor name");
  }
  string tensorName = content->lexer.getIdentifier();
  consume(Token::identifier);

  vector<Var> varlist;
  if (content->currentToken == Token::underscore) {
    consume(Token::underscore);
    if (content->currentToken == Token::lcurly) {
      consume(Token::lcurly);
      varlist = parseVarList();
      consume(Token::rcurly);
    }
    else {
      varlist.push_back(parseVar());
    }
  }
  else if (content->currentToken == Token::lparen) {
    consume(Token::lparen);
    varlist = parseVarList();
    consume(Token::rparen);
  }

  Format format;
  if (util::contains(content->formats, tensorName)) {
    format = content->formats.at(tensorName);
  }
  else {
    Format::LevelTypes      levelTypes;
    Format::DimensionOrders dimensions;
    size_t order = varlist.size();
    for (size_t i = 0; i < order; i++) {
      // defaults
      levelTypes.push_back(LevelType::Dense);
      dimensions.push_back(i);
    }
    format = Format(levelTypes, dimensions);
  }

  TensorBase tensor;
  if (util::contains(content->tensors, tensorName)) {
    tensor = content->tensors.at(tensorName);
  }
  else {
    vector<int> dimensionSizes(format.getLevels().size());
    vector<bool> dimensionDefault(dimensionSizes.size(), false);
    for (size_t i = 0; i < dimensionSizes.size(); i++) {
      if (util::contains(content->dimensionSizes, tensorName)) {
        dimensionSizes[i] = content->dimensionSizes.at(tensorName)[i];
      }
      else if (util::contains(content->indexVarSizes, varlist[i])) {
        dimensionSizes[i] = content->indexVarSizes.at(varlist[i]);
      }
      else {
        dimensionSizes[i] = content->dimensionDefault;
        dimensionDefault[i] = true;
      }
    }
    tensor = TensorBase(tensorName, ComponentType::Double,
                        dimensionSizes, format, DEFAULT_ALLOC_SIZE);

    for (size_t i = 0; i < dimensionSizes.size(); i++) {
      if (dimensionDefault[i]) {
        content->defaultDimension.insert({tensor, i});
      }
    }

    content->tensors.insert({tensorName,tensor});
  }
  return Read(tensor, varlist);
}

vector<Var> Parser::parseVarList() {
  vector<Var> varlist;
  varlist.push_back(parseVar());
  while (content->currentToken == Token::comma) {
    consume(Token::comma);
    varlist.push_back(parseVar());
  }
  return varlist;
}

Var Parser::parseVar() {
  if (content->currentToken != Token::identifier) {
    throw ParseError("Expected index variable");
  }
  Var var = getIndexVar(content->lexer.getIdentifier());
  consume(Token::identifier);
  return var;
}

bool Parser::hasIndexVar(std::string name) const {
  return util::contains(content->indexVars, name);
}

Var Parser::getIndexVar(string name) const {
  taco_iassert(name != "");
  if (!hasIndexVar(name)) {
    Var var(name, (content->parsingLhs ? Var::Free : Var::Sum));
    content->indexVars.insert({name, var});

    // dimensionSizes can also store index var sizes
    if (util::contains(content->dimensionSizes, name)) {
      content->indexVarSizes.insert({var, content->dimensionSizes.at(name)[0]});
    }
  }
  return content->indexVars.at(name);
}

bool Parser::hasTensor(std::string name) const {
  return util::contains(content->tensors, name);
}

const TensorBase& Parser::getTensor(string name) const {
  taco_iassert(name != "");
  if (!hasTensor(name)) {
    taco_uerror << "Parser error: Tensor name " << name <<
        " not found in expression" << endl;
  }
  return content->tensors.at(name);
}

const std::map<std::string,TensorBase>& Parser::getTensors() const {
  return content->tensors;
}

string Parser::currentTokenString() {
  return (content->currentToken == Token::identifier)
      ? content->lexer.getIdentifier()
      : content->lexer.tokenString(content->currentToken);
}

void Parser::consume(Token expected) {
  if(content->currentToken != expected) {
    string error = "Expected \'" + content->lexer.tokenString(expected) +
                   "\' but got \'" + currentTokenString() + "\'";
    throw ParseError(error);
  }
  nextToken();
}

void Parser::nextToken() {
  content->currentToken = content->lexer.getToken();
}

}}
