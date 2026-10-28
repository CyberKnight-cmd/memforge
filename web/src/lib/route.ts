// Hash routes look like #/paging?preset=ac4&mode=compare; labs read their starting state here.
export function routeName(): string {
  return location.hash.replace(/^#\/?/, '').split('?')[0];
}
export function query(): URLSearchParams {
  return new URLSearchParams(location.hash.split('?')[1] ?? '');
}
