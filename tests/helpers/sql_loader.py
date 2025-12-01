import re


def _adapt_positional_to_psycopg(sql_text: str) -> str:
    """Convert $1, $2 ... placeholders to %s for psycopg2."""
    return re.sub(r"\$\d+", "%s", sql_text)
