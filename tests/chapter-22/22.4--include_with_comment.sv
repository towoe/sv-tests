/*
:name: 22.4--include_with_comment
:description: Test
:should_fail: 0
:tags: 22.4
:type: preprocessing parsing
*/
`include <dummy_include.sv> // comments after `include are perfectly legal
module top ();
endmodule
