describe('HTTP endpoints', () => {
  test('returns the expected response for a long endpoint test name', () => { expect(2 + 2).toBe(4); });
  test('returns a useful failure', () => { console.log('Node response payload'); expect({status:200}).toEqual({status:418}); });
  test.skip('skipped endpoint', () => {});
});
