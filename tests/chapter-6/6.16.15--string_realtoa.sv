/*
:name: string_realtoa
:description: string.realtoa()  tests
:should_fail: 0
:tags: 6.16.15
*/
module top();
	string a;
	initial
		a.realtoa(4.76);
endmodule
