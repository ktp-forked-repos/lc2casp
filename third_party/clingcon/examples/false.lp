#include "csp.lp".

&dom {1..10} = a.
&sum {a} > b.
&sum {b} > a.


&show {a}.
&show {b}.
