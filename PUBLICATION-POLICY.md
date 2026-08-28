# Publication policy

This repository is limited to independently written documentation, source
examples, functional API descriptions, and summarized runtime observations.
Its publication boundary is intentionally narrower than the complete private
research workspace.

## Material that may be published

- independently written C++ source and explanatory documentation;
- callable API names and behavior needed for interoperability;
- codec, buffer, and lifecycle information needed to reproduce the examples;
- summarized static conclusions without image addresses, private import
  identifiers, numeric kernel request values, or extracted implementation
  code; and
- runtime results, error behavior, and hashes of independently produced test
  packages when they do not disclose proprietary content.

## Material that must not be published

- platform SDK files, firmware or runtime modules, retail executables, or
  extracted implementation code;
- import metadata containing private identifiers, even when generated locally;
- analysis databases, memory dumps, crash dumps, raw private logs, screenshots,
  or photographs of analysis material;
- signing material, credentials, keys, tokens, or other access-control data;
- proprietary media, fixtures, icons, or other test assets; and
- third-party source unless its license and required attribution have been
  independently verified.

The repository ignore rules cover common forms of these artifacts. Ignore
rules are only a guardrail: every staged change must still be reviewed before
publication, including reachable Git history and release assets.

## Independent legal review

This project is not represented as legally cleared. Before relying on it as
such, the repository owner should obtain advice from qualified counsel in the
relevant jurisdiction with experience in software interoperability,
copyright, trade-secret law, and DMCA section 1201.

Counsel should privately review:

1. authorization and provenance for every material used during the research;
2. whether the published interfaces and observations are functional facts,
   protectable expression, or potentially confidential information;
3. whether any access-control activity falls within an applicable statutory
   exception or exemption;
4. the independently written examples and their dependency/import workflow;
5. third-party licenses, attribution, trademarks, and platform terms; and
6. the complete reachable Git history and GitHub release assets.

Acquisition details and private source materials should be supplied directly
to counsel rather than added to this public repository. A legal disclaimer or
neutral wording is not a substitute for that review.
