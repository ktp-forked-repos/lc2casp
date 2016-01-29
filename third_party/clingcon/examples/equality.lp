#include "csp.lp".


&dom{1..10} = a.
&dom{1..10} = b.

&sum {2*a;3} = b.
&sum {a} = 3*c-4.


&show {a}.
&show {b}.
&show {c}.

