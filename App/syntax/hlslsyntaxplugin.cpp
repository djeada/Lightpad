#include "hlslsyntaxplugin.h"
#include <QRegularExpression>

QStringList HlslSyntaxPlugin::getPrimaryKeywords() {
  return {
      "void",    "bool",    "int",     "uint",    "float",   "double",
      "half",    "min16float", "min10float", "min16int", "min12int",
      "min16uint",
      "bool1",   "bool2",   "bool3",   "bool4",
      "int1",    "int2",    "int3",    "int4",
      "uint1",   "uint2",   "uint3",   "uint4",
      "float1",  "float2",  "float3",  "float4",
      "double1", "double2", "double3", "double4",
      "half1",   "half2",   "half3",   "half4",
      "float1x1","float1x2","float1x3","float1x4",
      "float2x1","float2x2","float2x3","float2x4",
      "float3x1","float3x2","float3x3","float3x4",
      "float4x1","float4x2","float4x3","float4x4",
      "matrix",  "vector",  "string",
      "Buffer",  "ByteAddressBuffer", "RWByteAddressBuffer",
      "StructuredBuffer", "RWStructuredBuffer", "AppendStructuredBuffer",
      "ConsumeStructuredBuffer",
      "Texture1D", "Texture2D", "Texture3D", "TextureCube",
      "Texture1DArray", "Texture2DArray", "TextureCubeArray",
      "Texture2DMS", "Texture2DMSArray",
      "RWTexture1D", "RWTexture2D", "RWTexture3D",
      "RWTexture1DArray", "RWTexture2DArray",
      "SamplerState", "SamplerComparisonState",
      "InputPatch", "OutputPatch"};
}

QStringList HlslSyntaxPlugin::getSecondaryKeywords() {
  return {"if",       "else",      "for",       "while",     "do",
          "switch",   "case",      "default",   "break",     "continue",
          "return",   "discard",   "struct",    "cbuffer",   "tbuffer",
          "typedef",  "namespace", "class",     "interface", "template",
          "in",       "out",       "inout",     "uniform",   "const",
          "static",   "extern",    "inline",    "precise",   "shared",
          "groupshared", "volatile", "register", "packoffset",
          "row_major","column_major"};
}

QStringList HlslSyntaxPlugin::getTertiaryKeywords() {
  return {"true",    "false",   "NULL",
          "SV_Position", "SV_Target", "SV_Depth", "SV_VertexID",
          "SV_InstanceID", "SV_PrimitiveID", "SV_Coverage",
          "SV_IsFrontFace", "SV_SampleIndex", "SV_GroupID",
          "SV_GroupIndex", "SV_GroupThreadID", "SV_DispatchThreadID",
          "SV_OutputControlPointID", "SV_TessFactor", "SV_InsideTessFactor",
          "SV_DomainLocation", "SV_GSInstanceID",
          "POSITION", "NORMAL", "TEXCOORD", "COLOR", "TANGENT", "BINORMAL",
          "BLENDINDICES", "BLENDWEIGHT", "PSIZE", "FOG", "TESSFACTOR",
          "VFACE", "VPOS",
          "vertex", "pixel", "geometry", "hull", "domain", "compute"};
}

QVector<SyntaxRule> HlslSyntaxPlugin::syntaxRules() const {
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
    rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
    rule.name = "keyword_2";
    rules.append(rule);
  }

  SyntaxRule preprocessorRule;
  preprocessorRule.pattern =
      QRegularExpression("^\\s*#\\s*(define|undef|if|ifdef|ifndef|elif|else|"
                         "endif|pragma|error|include|line)\\b");
  preprocessorRule.name = "preprocessor_directive";
  rules.append(preprocessorRule);

  SyntaxRule numberRule;
  numberRule.pattern =
      QRegularExpression("\\b[-+]?\\d+\\.?\\d*([eE][+-]?\\d+)?[uUfFlLhH]?\\b");
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

QVector<MultiLineBlock> HlslSyntaxPlugin::multiLineBlocks() const {
  QVector<MultiLineBlock> blocks;

  MultiLineBlock commentBlock;
  commentBlock.startPattern = QRegularExpression("/\\*");
  commentBlock.endPattern = QRegularExpression("\\*/");
  blocks.append(commentBlock);

  return blocks;
}

QStringList HlslSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
