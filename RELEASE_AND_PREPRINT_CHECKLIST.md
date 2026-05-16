# Release and Preprint Checklist

1. Run repository verification:
   ```bash
   pip install -r requirements.txt
   python scripts/verify_results.py
   ```
   Confirm `REPO_VERIFICATION_LOG.txt` shows `PASS`.

2. Commit final metadata changes only (do not include regenerated plot PDFs unless intentionally refreshed).

3. Create GitHub tag `v1.0.0`.
   - Note: a local tag `v1.0.0` may already exist from an earlier release. If so, delete and recreate on the new commit after review:
     ```bash
     git tag -d v1.0.0
     git tag -a v1.0.0 -m "Reproducibility package for TAES submission"
     ```

4. Create GitHub release `v1.0.0` from the same commit as the tag.

5. Archive the GitHub release on Zenodo (enable Zenodo-GitHub integration if not already configured).

6. Copy the Zenodo DOI into `README.md` and `CITATION.cff`.

7. Upload the SSRN preprint PDF manually using files in `preprint/SSRN_SUBMISSION/`.

8. Add the SSRN link to `README.md` after upload.

9. Update CV and ORCID with the preprint and Zenodo record.

10. Do not claim peer-reviewed publication until the manuscript is accepted.
