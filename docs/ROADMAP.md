# Maintenance roadmap — after v3.0.6 feature completion

The feature milestone is complete: native Android, browser, encrypted text/media/receipts, Fast/Balanced/Max/Ultra, authenticated profile negotiations, compact Max/Ultra acknowledgments and requested Auto RSSI bands are present in source. The maintainer reports Auto works on two radios and one urban Max range test reached approximately 200–250 meters. This is not a formal field qualification or range guarantee.

## Next focus: qualification, not feature expansion

1. Reproducible ESP-IDF and Android CI target builds and signed upgrade process.
2. Long-duration two-board Auto/lost-link field test with release-tagged diagnostics.
3. Browser↔Android combinations, reconnect/duplicate/receipt/media tests.
4. Persistent security replay controls, interrupted key rotation and BLE threat review.
5. Jurisdiction-specific spectrum and airtime rules; enforce required limits before deployment.
6. Formal release notes, issue triage, contributor onboarding and public source maintenance.

**Deferred by owner request:** battery/low-power optimization. No multi-hop routing or hardware redesign is implied in this source-complete milestone. Historical phase-by-phase roadmap: [`history/ROADMAP_BEFORE_PUBLICATION.md`](history/ROADMAP_BEFORE_PUBLICATION.md).
