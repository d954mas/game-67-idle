# pg_dump and a server for the restore check from the database image,
# rclone for the bucket, curl for the heartbeat.
FROM postgres:18-alpine
RUN apk add --no-cache rclone curl
COPY --chmod=755 backup.sh /backup.sh
ENTRYPOINT ["/backup.sh"]
CMD ["loop"]
