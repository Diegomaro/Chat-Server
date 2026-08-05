# Rate limiting

* Block users from re-requesting communication to users that have already denied conversation in N amount of time.
* Block users from requesting more than 3 connections. Cooldown of 1 minute.
* Error type of communication will make users to be on cooldown.
* Restrict how many bytes can be sent from the client/server before receiving acknowledgements.