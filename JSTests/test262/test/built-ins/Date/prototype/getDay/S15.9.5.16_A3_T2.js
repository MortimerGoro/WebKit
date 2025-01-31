// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: |
    The Date.prototype.getDay property "length" has { ReadOnly, ! DontDelete,
    DontEnum } attributes
es5id: 15.9.5.16_A3_T2
description: Checking DontDelete attribute
---*/

if (delete Date.prototype.getDay.length !== true) {
  throw new Test262Error('#1: The Date.prototype.getDay.length property does not have the attributes DontDelete');
}

if (Date.prototype.getDay.hasOwnProperty('length')) {
  throw new Test262Error('#2: The Date.prototype.getDay.length property does not have the attributes DontDelete');
}
