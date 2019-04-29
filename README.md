# simdcsv
A fast SIMD parser for CSV files as defined by RFC 4180.

This project will be a fast SIMD parser for CSV files. The approach will closely resemble https://github.com/lemire/simdjson in many respects; I plan to a number of similar tricks to the ones we did in that project. Initially, many techniques will be (regrettably) copy-pasted from that project; I hope to factor out some common functionality for this kind of code later.

Real-life parsing of CSV files has to deal with a huge range of optional variations on what a CSV might look like. My plan is to initially focus on standards-compliant CSV files and potentially add some variations later.

The broad outline of how the parsing will work:

1) Read in a CSV file into a buffer - as per usual, the buffer will be cache-line-aligned and padded so that even an exuberantly long SIMD read in a unrolled loop can safely happen without having to worry about unsafe reads.

2) Identification of CSV fields. This process will be considerably simpler, as unlike simdjson, we will not have to a implement a complex grammar.

a) We need to identify where are quotes are *first* - this ensures that escaped commas and CR-LF pairs are not treated as separators. Since RFC 4180 defines our quote convention as using "" for an escaped quote in all circumstances where they appear, and otherwise pairing quotes at the start and end of a field, this means that our quote detection code from simjson (see https://branchfree.org/2019/03/06/code-fragment-finding-quote-pairs-with-carry-less-multiply-pclmulqdq/ for a write-up) will allow us to identify all regions where we are 'inside' a quote quite easily.

The "edges" that we will identify here are relatively complex as we will nominally leave and reenter a quoted field every time we encounter a doubled-quote. So for example, 
```
,"foo""bar,",
```
encountered in a field will cause us to 'leave and renenter' our quoted field between the 'foo' and the 'bar'. However, this will have no real effect on the main point of this pass, which is to identify unescaped commas and CR-LF sequences.

3) Comma and CR-LF detection.

We need to then scan for commas and CR-LF pairs. This is relatively simple and the only new wrinkle on SIMD scanning techniques in simdjson is the fact that we have to detect a CR followed by a LF. 

At this point, we can identify all our actual delimiters. There may be additional passes to be done in the SIMD domain, but it's possible that we might at this stage do a bits-to-indexes transform and start working on our CSV document as a series of indexes into our data in a 2-dimensional (at least nominally) array.


Other tasks that need to happen:

- We should validate that the things that appear as "textdata" within the fields are valid ASCII as per the standard.
- UTF validation is not covered by RFC 4180 but will surely be a necessity.
- Numbers that appear within fields will likely need to be converted to integer or floating point values
- The escaped text will need to be converted (in situ or in newly allocated storage) into unescaped variants

The initial cut of the code will be for AVX2 capable machines. An ARM variant will appear shortly, as will AVX512 and possible SSE versions.
