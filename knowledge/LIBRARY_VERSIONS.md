# Library Versions - Local Copies

**Date:** 2025-10-11
**Purpose:** Document exact versions of locally-cloned libraries for version control

---

## NativeEthernet

- **Repository:** https://github.com/vjmuzik/NativeEthernet
- **Commit:** `cdf6b3a2b5d62559a7c76be5b9d3fa60cf9d686b`
- **Author:** Tino Hernandez <vjmuzik1@gmail.com>
- **Date:** 2021-09-03 16:15:10 -0400
- **Message:** Fix UDP write incorrect socket size
- **Location:** `lib/NativeEthernet/`
- **Status:** Frozen - no upstream updates expected

**Notes:**
- Last commit was September 2021 (~3.5 years ago)
- Library appears unmaintained but stable
- Local copy ensures no breaking changes
- Can apply local patches if needed

---

## FNET (TCP/IP Stack)

- **Repository:** https://github.com/vjmuzik/FNET
- **Commit:** `be3a67e4a568bd1888533007ab218911e92d7719`
- **Author:** Tino Hernandez <vjmuzik1@gmail.com>
- **Date:** 2021-08-14 12:45:20 -0400
- **Message:** Fix DNS client not resolving CNAME from host
- **Location:** `lib/FNET/`
- **Status:** Frozen - no upstream updates expected

**Notes:**
- Last commit was August 2021 (~3.5 years ago)
- Fork of original FNET project adapted for Teensy
- Provides TCP/IP stack for NativeEthernet
- Much lighter weight than lwIP (~80 KB RAM savings)

---

## Version Control Strategy

Both libraries are:
- ✅ Cloned locally into `lib/` directory
- ✅ Tracked in project git repository
- ✅ Frozen at known-good commits
- ✅ Protected from upstream breaking changes
- ✅ Available for local modifications

**Rationale:**
Since both libraries are unmaintained (last updated 2021), local copies ensure:
1. Build reproducibility
2. Protection from repository deletion
3. Ability to apply custom patches
4. Full control over dependency versions

---

## Migration Context

**Replacing:**
- QNEthernet (actively maintained, based on lwIP)
- lwIP TCP/IP stack (~140 KB RAM usage)

**With:**
- NativeEthernet (unmaintained but stable)
- FNET TCP/IP stack (~60 KB RAM usage)

**Expected Savings:** ~80 KB RAM

---

## Future Considerations

If issues arise with these frozen versions:
1. Can cherry-pick specific commits from upstream
2. Can apply custom patches locally
3. Can consider alternative Ethernet libraries if needed
4. QNEthernet remains as rollback option in git history

**Upstream Repositories (Original):**
- FNET: https://github.com/fnet-stack/FNET (original project)
- NativeEthernet fork basis: Teensy forums community
