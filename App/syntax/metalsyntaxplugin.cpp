#include "metalsyntaxplugin.h"
#include <QRegularExpression>

QStringList MetalSyntaxPlugin::getPrimaryKeywords() {
  return {"void",
          "bool",
          "char",
          "uchar",
          "short",
          "ushort",
          "int",
          "uint",
          "long",
          "ulong",
          "half",
          "float",
          "size_t",
          "ptrdiff_t",
          "intptr_t",
          "uintptr_t",
          "char2",
          "char3",
          "char4",
          "uchar2",
          "uchar3",
          "uchar4",
          "short2",
          "short3",
          "short4",
          "ushort2",
          "ushort3",
          "ushort4",
          "int2",
          "int3",
          "int4",
          "uint2",
          "uint3",
          "uint4",
          "long2",
          "long3",
          "long4",
          "ulong2",
          "ulong3",
          "ulong4",
          "half2",
          "half3",
          "half4",
          "float2",
          "float3",
          "float4",
          "float2x2",
          "float2x3",
          "float2x4",
          "float3x2",
          "float3x3",
          "float3x4",
          "float4x2",
          "float4x3",
          "float4x4",
          "half2x2",
          "half2x3",
          "half2x4",
          "half3x2",
          "half3x3",
          "half3x4",
          "half4x2",
          "half4x3",
          "half4x4",
          "texture1d",
          "texture2d",
          "texture3d",
          "texturecube",
          "texture1d_array",
          "texture2d_array",
          "texturecube_array",
          "texture2d_ms",
          "texture2d_ms_array",
          "depth2d",
          "depthcube",
          "depth2d_array",
          "depthcube_array",
          "depth2d_ms",
          "depth2d_ms_array",
          "sampler",
          "array",
          "atomic_int",
          "atomic_uint"};
}

QStringList MetalSyntaxPlugin::getSecondaryKeywords() {
  return {"if",       "else",     "for",     "while",           "do",
          "switch",   "case",     "default", "break",           "continue",
          "return",   "struct",   "typedef", "using",           "namespace",
          "template", "class",    "inline",  "static",          "extern",
          "const",    "volatile", "auto",    "register",        "enum",
          "union",    "sizeof",   "typedef", "discard_fragment"};
}

QStringList MetalSyntaxPlugin::getTertiaryKeywords() {
  return {"true",
          "false",
          "NULL",
          "nullptr",
          "constant",
          "device",
          "threadgroup",
          "threadgroup_imageblock",
          "thread",
          "object_data",
          "ray_data",
          "patch_control_point",
          "kernel",
          "vertex",
          "fragment",
          "tile",
          "visible",
          "stitchable",
          "object",
          "mesh",
          "in",
          "out",
          "inout",
          "[[vertex_id]]",
          "[[instance_id]]",
          "[[position]]",
          "[[front_facing]]",
          "[[point_coord]]",
          "[[sample_id]]",
          "[[sample_mask]]",
          "[[coverage_mask]]",
          "[[thread_position_in_grid]]",
          "[[thread_position_in_threadgroup]]",
          "[[thread_index_in_threadgroup]]",
          "[[threads_per_threadgroup]]",
          "[[threads_per_grid]]",
          "[[threadgroup_position_in_grid]]",
          "[[threadgroups_per_grid]]",
          "[[dispatch_threads_per_threadgroup]]",
          "[[color]]",
          "[[depth]]",
          "[[stencil]]",
          "[[buffer]]",
          "[[texture]]",
          "[[sampler]]",
          "access::read",
          "access::write",
          "access::read_write",
          "access::sample"};
}

QVector<SyntaxRule> MetalSyntaxPlugin::syntaxRules() const {
  QVector<SyntaxRule> rules;

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
    if (keyword.startsWith("[[") || keyword.contains("::")) {
      rule.pattern = QRegularExpression(QRegularExpression::escape(keyword));
    } else {
      rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    }
    rule.name = "keyword_2";
    rules.append(rule);
  }

  SyntaxRule preprocessorRule;
  preprocessorRule.pattern =
      QRegularExpression("^\\s*#\\s*(include|define|undef|if|ifdef|ifndef|"
                         "elif|else|endif|pragma|error|line)\\b");
  preprocessorRule.name = "preprocessor_directive";
  rules.append(preprocessorRule);

  SyntaxRule numberRule;
  numberRule.pattern =
      QRegularExpression("\\b[-+]?\\d+\\.?\\d*([eE][+-]?\\d+)?[uUfFhH]?\\b");
  numberRule.name = "number";
  rules.append(numberRule);

  SyntaxRule stringRule;
  stringRule.pattern = QRegularExpression("\"[^\"]*\"");
  stringRule.name = "string";
  rules.append(stringRule);

  SyntaxRule functionRule;
  functionRule.pattern = QRegularExpression("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()");
  functionRule.name = "function";
  rules.append(functionRule);

  SyntaxRule commentRule;
  commentRule.pattern = QRegularExpression("//[^\n]*");
  commentRule.name = "comment";
  rules.append(commentRule);

  return rules;
}

QVector<MultiLineBlock> MetalSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList MetalSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
