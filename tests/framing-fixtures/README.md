# RTP framing fixtures

`fixtures.h` contains four locally generated, complete one-frame Annex-B streams
and independently extracted NAL offsets/lengths. No camera, vendor input, network,
FFmpeg or external project is needed to compile or run the tests. They include
small AVC/HEVC NALs and large AVC/HEVC IDR NALs that exercise RTP fragmentation.
All test NAL payloads are at least six bytes.

`provenance.json` records the original FFmpeg 8.0.1 build, exact encode/decode
commands, stream and individual NAL SHA256 values, and one-frame decoded output
MD5 values. The input is FFmpeg's `lavfi testsrc2` at 32x32 or 512x512, yuv420p,
one frame, using libx264/libx265 with fixed single-thread encoder options.
`<out>` in the recorded commands means a fresh local output directory. Generation
is optional provenance work; encoding may differ with another encoder build.

For the header, read each recorded stream as bytes, locate 3/4-byte Annex-B
prefixes in order, and define each NAL slice after its prefix through the next
prefix (excluding zero padding at the end of that slice). `frames[].nals` gives
the exact frozen offsets, lengths, types and hashes used by this test. The C
header serializes each complete stream in a `static const unsigned char[]` and
its slice table in `struct fixture_nal[]`; its hash is in the provenance file.
The payload oracle uses these frozen slices, not the production NAL splitter.

Run from the repository root:

```
make -C tests test
```

This runs native and ASan/UBSan tests for the accepted parameter-cache behavior
and the actual public RTP packetizer. `test-native`, `test-sanitize`,
`test-framing-native` and `test-framing-sanitize` are available separately.
`BUILD_DIR` may point at a writable external build directory. Optionally set
`FRAMING_CAPTURE_DIR` to an existing directory to retain packet files; the test
refuses to replace an existing capture file.

The framing harness replaces UDP `send` with capture/error injection. A supplied
positive timestamp is required in every test; an aborting `millis` stub detects
any unexpected wall-clock fallback. It creates no sockets, server or workers.
The tests check byte-exact single/FU NAL reconstruction, sequence/timestamp wrap,
unchanged input and partial send failure. They do not test marker policy,
parameter-set replacement, concurrent calls, RTSP negotiation, actual network
delivery or playback. No unsafe short-MIME/SPS fault reproducer is included.
The existing VPS suite retains its corrected-code short-VPS rejection checks.
