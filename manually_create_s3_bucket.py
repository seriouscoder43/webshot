from pathlib import Path

from tests.helpers.s3_bucket import ensure_s3_bucket_exists

secrets_path = Path("secret/test_secdist.json")
ensure_s3_bucket_exists(secrets_path=secrets_path, endpoint="127.0.0.1:8333", bucket="webshot")
