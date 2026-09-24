// Execute the navigation functions from the shipped QML, without compiling.
import fs from 'node:fs';
import vm from 'node:vm';
import assert from 'node:assert/strict';
const source = fs.readFileSync(new URL('../resources/qml/Manager.qml', import.meta.url), 'utf8');
const names = ['rememberPage', 'restorePage', 'showPage', 'openEntry', 'closePage', 'back'];
const functions = names.map(name => {
    const start = source.indexOf(`    function ${name}(`);
    assert(start >= 0);
    let end = source.indexOf('{', start) + 1, depth = 1;
    while (depth) { if (source[end] === '{') depth++; if (source[end] === '}') depth--; end++; }
    return source.slice(start, end);
}).join('\n');
function manager(owner) {
    const state = { initialOwner: owner, pageHistory: [], selectedPage: {}, selectedPackage: {},
        mode: 'list', parent: {active: true}, message: '', visible: true,
        pages: [{id:'a',pageId:'main',available:true}, {id:'b',pageId:'main',available:true}],
        translate: text => text, QT_TR_NOOP: text => text };
    state.root = state;
    vm.createContext(state); vm.runInContext(functions, state);
    return state;
}
const inventory = manager('xovi-extension-manager');
inventory.openEntry('xovi-extension-manager', 'inventory', true);
inventory.mode = 'detail'; inventory.selectedPackage = {id:'a'};
inventory.openEntry('a', 'main'); inventory.back();
assert.equal(inventory.mode, 'detail'); assert.equal(inventory.selectedPackage.id, 'a');
assert.equal(inventory.selectedPage.id, undefined); assert.equal(inventory.parent.active, true);
inventory.mode = 'notifications'; inventory.openEntry('a', 'main'); inventory.closePage();
assert.equal(inventory.mode, 'notifications'); assert.equal(inventory.parent.active, true);
const shortcut = manager('a');
shortcut.openEntry('a', 'main', true);
shortcut.openEntry('b', 'main'); shortcut.closePage();
assert.equal(shortcut.selectedPage.id, 'a'); assert.equal(shortcut.parent.active, true);
shortcut.back(); assert.equal(shortcut.parent.active, false);
const direct = manager('a');
direct.showPage({id:'a',pageId:'main'}, true);
direct.showPage({id:'inline', transient:true}); direct.back();
assert.equal(direct.selectedPage.id, 'a'); assert.equal(direct.parent.active, true);
console.log('PASS inventory/detail, notification, nested plugin, direct page and shortcut return paths');
