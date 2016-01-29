#include "csp.lp".
#const n = 10.
vars(a;b).

&dom {1..n} = X :- vars(X).


&show {X : vars(X)}.


&minimize {X : vars(X)}.
