// Parse declarations without executing test bodies. Dynamic declarations remain
// discoverable as files and receive exact names and locations after a Jest run.
const fs = require('node:fs');
const path = require('node:path');
const { spawnSync } = require('node:child_process');

try {
  const jest = require.resolve('jest/bin/jest', { paths: [process.cwd()] });
  const listing = spawnSync(process.execPath, [jest, '--listTests', '--json', '--runInBand'],
    { cwd: process.cwd(), encoding: 'utf8', env: process.env, maxBuffer: 16 * 1024 * 1024 });
  if (listing.status !== 0) throw new Error(listing.stderr || listing.error || 'Jest discovery failed');
  const files = JSON.parse(listing.stdout);
  let parser;
  try { parser = require(require.resolve('@babel/parser', { paths: [process.cwd(), path.dirname(jest)] })); }
  catch (_) { /* File-level discovery still works without a source parser. */ }
  const tests = [];
  const title = node => node?.type === 'StringLiteral' ? node.value
    : node?.type === 'TemplateLiteral' && node.expressions.length === 0 ? node.quasis[0].value.cooked : null;
  const callee = node => node?.type === 'Identifier' ? node.name
    : node?.type === 'MemberExpression' && ['only', 'skip', 'todo'].includes(node.property?.name) ? callee(node.object) : null;
  for (const file of files) {
    // A file record ensures parameterized/dynamic tests can always be run.
    tests.push({ id: file, name: path.basename(file), file, line: 1, isFile: true });
    if (!parser) continue;
    let ast;
    try { ast = parser.parse(fs.readFileSync(file, 'utf8'), { sourceType: 'unambiguous', plugins: ['jsx', 'typescript'] }); }
    catch (_) { continue; }
    function walk(node, ancestors = []) {
      if (!node || typeof node !== 'object') return;
      if (node.type === 'CallExpression') {
        const kind = callee(node.callee);
        const name = title(node.arguments[0]);
        if (kind === 'describe') {
          if (name !== null) walk(node.arguments[1], [...ancestors, name]);
          return;
        }
        if (kind === 'test' || kind === 'it') {
          if (name !== null) {
            const fullName = [...ancestors, name].join(' ');
            tests.push({ id: file + '::' + fullName, name: fullName, file, line: node.loc.start.line });
          }
          return;
        }
        // An .each callback has runtime-generated names; don't invent IDs for it.
        if (node.callee?.type === 'CallExpression' || node.callee?.type === 'TaggedTemplateExpression') return;
      }
      for (const [key, value] of Object.entries(node)) {
        if (key === 'loc' || key === 'extra') continue;
        if (Array.isArray(value)) value.forEach(child => walk(child, ancestors));
        else if (value && typeof value === 'object') walk(value, ancestors);
      }
    }
    walk(ast);
  }
  process.stdout.write(JSON.stringify(tests));
} catch (error) {
  process.stderr.write(String(error.message || error) + '\n');
  process.exitCode = 1;
}
