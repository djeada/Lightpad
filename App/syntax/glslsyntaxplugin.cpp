#include "glslsyntaxplugin.h"
#include <QRegularExpression>

QStringList GlslSyntaxPlugin::getPrimaryKeywords() {
  return {"void",
          "bool",
          "int",
          "uint",
          "float",
          "double",
          "bvec2",
          "bvec3",
          "bvec4",
          "ivec2",
          "ivec3",
          "ivec4",
          "uvec2",
          "uvec3",
          "uvec4",
          "vec2",
          "vec3",
          "vec4",
          "dvec2",
          "dvec3",
          "dvec4",
          "mat2",
          "mat3",
          "mat4",
          "mat2x2",
          "mat2x3",
          "mat2x4",
          "mat3x2",
          "mat3x3",
          "mat3x4",
          "mat4x2",
          "mat4x3",
          "mat4x4",
          "dmat2",
          "dmat3",
          "dmat4",
          "dmat2x2",
          "dmat2x3",
          "dmat2x4",
          "dmat3x2",
          "dmat3x3",
          "dmat3x4",
          "dmat4x2",
          "dmat4x3",
          "dmat4x4",
          "sampler2D",
          "sampler3D",
          "samplerCube",
          "sampler2DShadow",
          "samplerCubeShadow",
          "sampler2DArray",
          "sampler2DArrayShadow",
          "isampler2D",
          "isampler3D",
          "isamplerCube",
          "isampler2DArray",
          "usampler2D",
          "usampler3D",
          "usamplerCube",
          "usampler2DArray",
          "sampler2DMS",
          "isampler2DMS",
          "usampler2DMS",
          "sampler2DMSArray",
          "isampler2DMSArray",
          "usampler2DMSArray",
          "image2D",
          "iimage2D",
          "uimage2D",
          "image3D",
          "iimage3D",
          "uimage3D",
          "imageCube",
          "iimageCube",
          "uimageCube",
          "image2DArray",
          "iimage2DArray",
          "uimage2DArray",
          "atomic_uint"};
}

QStringList GlslSyntaxPlugin::getSecondaryKeywords() {
  return {"if",      "else",    "for",       "while",    "do",      "switch",
          "case",    "default", "break",     "continue", "return",  "discard",
          "struct",  "in",      "out",       "inout",    "uniform", "attribute",
          "varying", "const",   "layout",    "flat",     "smooth",  "centroid",
          "sample",  "patch",   "subroutine"};
}

QStringList GlslSyntaxPlugin::getTertiaryKeywords() {
  return {"true",
          "false",
          "precision",
          "highp",
          "mediump",
          "lowp",
          "invariant",
          "precise",
          "coherent",
          "volatile",
          "restrict",
          "readonly",
          "writeonly",
          "shared",
          "buffer",
          "gl_Position",
          "gl_FragCoord",
          "gl_FragColor",
          "gl_FragDepth",
          "gl_VertexID",
          "gl_InstanceID",
          "gl_PointSize",
          "gl_PointCoord",
          "gl_FrontFacing",
          "gl_Layer",
          "gl_ViewportIndex",
          "gl_WorkGroupSize",
          "gl_WorkGroupID",
          "gl_LocalInvocationID",
          "gl_GlobalInvocationID",
          "gl_LocalInvocationIndex"};
}

QVector<SyntaxRule> GlslSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

  // Lower priority than keywords: a type before a variable/parameter name.
  SyntaxRule declaredType;
  declaredType.pattern = QRegularExpression(
      R"(\b([A-Za-z_]\w*)(?=\s+(?:const\s+)?[A-Za-z_]\w*\s*[;=,()\[{}]))");
  declaredType.name = "type_reference";
  declaredType.captureGroup = 1;
  rules.append(declaredType);

  // A bare "left * right" is also an expression. Only infer pointer types
  // from a type-like spelling; declared local names are resolved by the index.
  SyntaxRule indirectType;
  indirectType.pattern = QRegularExpression(
      R"(\b([A-Z][A-Za-z0-9_]*|[A-Za-z_]\w*_t)(?=\s*(?:const\s+)?[*&]+\s*[A-Za-z_]\w*\s*[;=,()\[{}]))");
  indirectType.name = "type_reference";
  indirectType.captureGroup = 1;
  rules.append(indirectType);

  for (const QString &keyword : getPrimaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_0";
    rules.append(rule);
  }

  for (const QString &keyword : getSecondaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_1";
    rules.append(rule);
  }

  for (const QString &keyword : getTertiaryKeywords()) {
    SyntaxRule rule;
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_2";
    rules.append(rule);
  }

  SyntaxRule preprocessorRule;
  preprocessorRule.pattern =
      QRegularExpression("^\\s*#\\s*(version|extension|define|undef|if|ifdef|"
                         "ifndef|elif|else|endif|pragma|error|line)\\b");
  preprocessorRule.name = "preprocessor_directive";
  rules.append(preprocessorRule);

  SyntaxRule numberRule;
  numberRule.pattern =
      QRegularExpression("\\b[-+]?\\d+\\.?\\d*([eE][+-]?\\d+)?[uUfF]?\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression(R"lp("(?:\\.|[^"\\])*")lp");
  stringRule.name = "string";
  rules.append(stringRule);

  // Method calls are refined by the function rule below; field accesses keep
  // the member color, including chains such as mesh.position.x.
  SyntaxRule memberRule;
  memberRule.pattern = QRegularExpression(
      R"((?:(?<!\.)\.(?!\.)|->)\s*([A-Za-z_$][A-Za-z0-9_$]*))");
  memberRule.name = "member";
  memberRule.captureGroup = 1;
  rules.append(memberRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression(
      QStringLiteral("\\b(?!(?:%1)\\b)[A-Za-z_][A-Za-z0-9_]*(?=\\s*\\()")
          .arg(keywords().join('|')));
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  // Capture the declared name, keeping the keyword in its own color.
  SyntaxRule typeDefinition0;
  typeDefinition0.pattern = QRegularExpression(R"(\bstruct\s+([A-Za-z_]\w*))");
  typeDefinition0.name = "type_definition";
  typeDefinition0.captureGroup = 1;
  rules.append(typeDefinition0);

  return rules;
}

QVector<MultiLineBlock> GlslSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList GlslSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
