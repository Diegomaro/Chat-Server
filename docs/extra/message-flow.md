# Message sending

* Improve message sending from server, allocate 1024 spaces for sending messages to not have a bottleneck caused by pending sends.
* Store pending messages in file before sending.
  - This allows for a priority queue. Decrease impact of malicious users sending messages constantly.
* Server only stores messages for some time.
