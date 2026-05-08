#include "wgslsyntaxplugin.h"
#include <QRegularExpression>

QStringList WgslSyntaxPlugin::getPrimaryKeywords() {
  return {"bool",    "i32",     "u32",     "f32",     "f16",
          "vec2",    "vec3",    "vec4",    "vec2i",   "vec3i",   "vec4i",
          "vec2u",   "vec3u",   "vec4u",   "vec2f",   "vec3f",   "vec4f",
          "vec2h",   "vec3h",   "vec4h",   "mat2x2",  "mat2x3",  "mat2x4",
          "mat3x2",  "mat3x3",  "mat3x4",  "mat4x2",  "mat4x3",  "mat4x4",
          "mat2x2f", "mat2x3f", "mat2x4f", "mat3x2f", "mat3x3f", "mat3x4f",
          "mat4x2f", "mat4x3f", "mat4x4f",
          "array",   "atomic",  "ptr",     "texture_1d", "texture_2d",
          "texture_2d_array", "texture_3d", "texture_cube",
          "texture_cube_array", "texture_multisampled_2d",
          "texture_storage_1d", "texture_storage_2d",
          "texture_storage_2d_array", "texture_storage_3d",
          "texture_depth_2d", "texture_depth_2d_array",
          "texture_depth_cube", "texture_depth_cube_array",
          "texture_depth_multisampled_2d",
          "sampler", "sampler_comparison"};
}

QStringList WgslSyntaxPlugin::getSecondaryKeywords() {
  return {"if",         "else",       "loop",       "for",
          "while",      "break",      "continue",   "return",
          "switch",     "case",       "default",    "discard",
          "continuing", "break_if",   "fn",         "struct",
          "var",        "let",        "const",      "type",
          "override",   "alias",      "enable",     "requires",
          "diagnostic", "const_assert"};
}

QStringList WgslSyntaxPlugin::getTertiaryKeywords() {
  return {"true",        "false",        "workgroup",  "uniform",
          "storage",     "private",      "function",   "read",
          "write",       "read_write",   "vertex",     "fragment",
          "compute",     "rgba8unorm",   "rgba8snorm", "rgba8uint",
          "rgba8sint",   "rgba16uint",   "rgba16sint", "rgba16float",
          "r32uint",     "r32sint",      "r32float",   "rg32uint",
          "rg32sint",    "rg32float",    "rgba32uint", "rgba32sint",
          "rgba32float", "bgra8unorm",
          "@vertex",     "@fragment",    "@compute",   "@binding",
          "@group",      "@location",    "@builtin",   "@size",
          "@align",      "@id",          "@interpolate",
          "@must_use",   "@diagnostic",  "@invariant", "@workgroup_size",
          "position",    "vertex_index", "instance_index",
          "front_facing","frag_depth",   "local_invocation_id",
          "local_invocation_index", "global_invocation_id",
          "workgroup_id","num_workgroups","sample_index","sample_mask"};
}

QVector<SyntaxRule> WgslSyntaxPlugin::syntaxRules() const {
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

  // Attributes (e.g. @vertex, @fragment) - highlight the @ + name
  SyntaxRule attrRule;
  attrRule.pattern = QRegularExpression("@[A-Za-z_][A-Za-z0-9_]*");
  attrRule.name = "keyword_2";
  rules.append(attrRule);

  for (const QString &keyword : getTertiaryKeywords()) {
    if (!keyword.startsWith('@')) {
      SyntaxRule rule;
      rule.pattern = QRegularExpression("\\b" + keyword + "\\b");
      rule.name = "keyword_2";
      rules.append(rule);
    }
  }

  SyntaxRule numberRule;
  numberRule.pattern =
      QRegularExpression("\\b[-+]?\\d+\\.?\\d*([eE][+-]?\\d+)?[uifjh]?\\b");
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

QVector<MultiLineBlock> WgslSyntaxPlugin::multiLineBlocks() const {
  return {};
}

QStringList WgslSyntaxPlugin::keywords() const {
  QStringList all;
  all << getPrimaryKeywords() << getSecondaryKeywords()
      << getTertiaryKeywords();
  return all;
}
