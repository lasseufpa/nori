# Sphinx configuration for the NORI manual.

project = "NORI"
author = "LASSE/UFPA"
copyright = "2026, LASSE/UFPA"
release = "3.00"
version = "3.00"

extensions = []

templates_path = []
exclude_patterns = []

master_doc = "index"
language = "en"
numfig = True
highlight_language = "c++"

html_theme = "alabaster"
html_theme_options = {
    "description": "O-RAN E2 interface for ns-3 and 5G-LENA",
    "fixed_sidebar": True,
    "page_width": "1100px",
    "sidebar_width": "240px",
}
html_static_path = ["_static"]
html_title = "NORI manual"

latex_documents = [
    ("index", "nori.tex", "NORI Manual", "LASSE/UFPA", "manual"),
]
