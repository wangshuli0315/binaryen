;; NOTE: Assertions have been generated by update_lit_checks.py --all-items and should not be edited.
;; NOTE: This test was ported using port_test.py and could be cleaned up.

;; RUN: foreach %s %t wasm-opt --local-cse -S -o - | filecheck %s

(module
 ;; testcase from AssemblyScript
 (func $div16_internal (param $0 i32) (param $1 i32) (result i32)
  (i32.add
   (i32.xor
    (i32.shr_s
     (i32.shl
      (local.get $0)
      (i32.const 16)
     )
     (i32.const 16)
    )
    (i32.shr_s
     (i32.shl
      (local.get $1)
      (i32.const 16)
     )
     (i32.const 16)
    )
   )
   (i32.xor
    (i32.shr_s
     (i32.shl
      (local.get $0)
      (i32.const 16)
     )
     (i32.const 16)
    )
    (i32.shr_s
     (i32.shl
      (local.get $1)
      (i32.const 16)
     )
     (i32.const 16)
    )
   )
  )
 )
)
