# Project 3 Design Log
**Author:** Om Panchal · **P2 code used:** my own · 

## 1. Hash table

I used separate chaining. Each bucket is a `std::vector<Entry>`, and each entry stores the tool name beside a `std::unique_ptr<Tool>`. A lookup hashes the full name, selects one bucket, and compares only the names in that bucket. Chaining was a good fit here because the registry never removes tools and I did not need tombstones or a special “table full” case. If a few keys collide, the table can still add them safely.

The hash is 64-bit FNV-1a. It processes every byte in order, so names that contain the same characters in a different order do not collapse to the same simple character sum. I reduce the result modulo the current bucket count only when selecting a bucket. The table starts with 8 buckets and doubles before an insertion would make the load factor exceed 1.0. A threshold of 1.0 is reasonable for chaining: it limits the average bucket length to at most one while avoiding resizing as frequently as a lower threshold. Rehashing recomputes every bucket index. Adding is amortized O(1): most insertions inspect one short chain, and the occasional O(n) rehash is spread over the insertions since the prior doubling.

For 10,000 keys named `tool_0` through `tool_9999`, my run produced 16,384 buckets, a load factor of 0.610352, and a longest chain of 6. As a comparison, the weak hash `s[0] + s.size()` produced a longest chain of 9,000 for the same keys: all names begin with `t`, and 9,000 of them have the same length. That comparison shows why considering all characters matters.

## 2. Pointer stability

The buckets own `unique_ptr`s, but the `Tool` objects are separately allocated. Rehashing moves each entry and its `unique_ptr` into a new bucket vector; it does not move the object that the pointer owns. Therefore a `Tool*` returned by `find()` continues to point to the same object after one or more resizes. The registry deletes each object exactly once when its owning pointer is destroyed. A duplicate name is rejected before insertion, so the original stays registered and the newly passed object is destroyed.

## 3. Calculator trace

For `2 * (3 + 4) - 5`, the values stack is written bottom-to-top. Reading `2` pushes it, then `*` goes on the operator stack. `(` is pushed; `3` and `4` are pushed as values, with `+` between them. At `)`, I apply `+`, popping 4 as the right operand and 3 as the left operand, and push 7. Then I remove `(`. When `-` arrives, the pending `*` has higher precedence, so I apply it to 2 and 7 and push 14. I push `-`, then read 5. At the end I apply `14 - 5`, leaving 9 as the answer. Equal-precedence operators are also applied before a new one is pushed, which makes subtraction and division evaluate left to right.

## 4. Errors as values

An invalid path, missing file, or division by zero is an expected input outcome in this harness. Returning `false` with a message lets the caller turn that outcome into a tool result the model can read and respond to. Throwing for routine input errors would make normal control flow harder to follow and would add exception handling around every expected failure. Exceptions still make sense for programming failures such as allocation errors or violated internal assumptions; they signal a bug or a condition the current operation cannot normally report. The evaluator and path resolver also check their stacks and paths before using them, so malformed input fails safely.

## 5. What I would design differently

For this small registry, vectors make the representation and rehash logic easy to inspect. If tools were removed frequently, I would revisit the representation and define deletion behavior, especially with open addressing. If the registry needed predictable worst-case lookup time under adversarial names, I would consider a collision-resistant hash or a balanced tree, while keeping the same public interface. For the current fixed workload, chaining with FNV-1a is simple, meets the measured probe bound, and keeps object ownership clear.
