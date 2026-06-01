# a minimal dockerfile to run clang-format-22, in case the user doesn't have it installed

FROM alpine:edge@sha256:9a341ff2287c54b86425cbee0141114d811ae69d88a36019087be6d896cef241

RUN apk add --no-cache clang22-extra-tools=22.1.3-r0

ENTRYPOINT ["clang-format"]
