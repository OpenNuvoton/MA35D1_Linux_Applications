# MA35D1 OTP Version Counter Utility

`otp_vcnt` reads the BL2, FIP, and Linux OTP version counters through the
MA35D1 Key Store driver. It can also advance the FIP and Linux counters for
anti-rollback protection.

OTP programming is permanent. Test updates on a development device first and
make sure the requested version is correct before running an update command.

## Build

Build the utility with the AArch64 cross compiler:

```sh
make
```

The output executable is `otp_vcnt`. The target system must provide
`/dev/ksdev` and include the matching Key Store driver and OP-TEE PTA support.

## Read version counters

Run either command to read all three counters:

```sh
./otp_vcnt
./otp_vcnt read
```

Example output:

```text
BL2 OTP version counter:   3
FIP OTP version counter:   5
Linux OTP version counter: 7
```

The BL2 OTP version counter is managed automatically by BL2 according to the
version number embedded in the BL2 image. This utility only reads the BL2
counter and does not provide a command to update it.

## Update the FIP or Linux counter

Advance a counter to a specified version with:

```sh
./otp_vcnt update fip <version>
./otp_vcnt update linux <version>
```

For example:

```sh
./otp_vcnt update fip 6
./otp_vcnt update linux 8
```

Each counter supports values from 0 through 352. An update can only keep or
increase the current value; a lower version is rejected as a rollback. After
programming, the utility reads the counter again and verifies the new value.

There is intentionally no `update bl2` command. Install a BL2 image with the
appropriate embedded version and let BL2 manage its own OTP version counter.
