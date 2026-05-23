# noirajjo

A fork of [yggdrasil-network/yggdrasil-go](https://github.com/yggdrasil-network/yggdrasil-go), modified for use in Bangladesh.

## Background

In July–August 2024, Bangladesh experienced coordinated internet shutdowns during the political unrest surrounding the student-led protests against the Awami League government. Access was cut at the border gateway protocol level — not just social media blocks, but full disruption of international routing. People inside the country couldn't reach the outside, and coordination between people within the country became difficult.

Yggdrasil is a peer-to-peer encrypted mesh network. It runs over existing ISP infrastructure — it doesn't need international routing to function. Two people on the same ISP, or connected through any chain of peers, can communicate through Yggdrasil even when their BGP connections to the rest of the world are severed.

Noirajjo (নয়রাজ্য — "no state" / "anarchy") is this fork, configured with Bangladesh's network topology in mind.

## What Yggdrasil does

Yggdrasil builds a self-organising encrypted mesh where every node gets a stable IPv6 address derived from its public key. Traffic is routed through the mesh without any central coordination. It runs over TCP/TLS on top of whatever connectivity exists — if two nodes can reach each other through local ISP infrastructure, they can communicate.

The key property for shutdown scenarios: it doesn't need the internet to function, just IP connectivity between peers. Local ISPs continued to route traffic between customers even during the BGP shutdowns.

## Modifications

This fork adjusts default peer configuration and bootstrapping to work better with Bangladeshi ISP topology. Rather than relying on international bootstrap peers (which become unreachable during a shutdown), it is configured with local peer priorities.

See the original repo for all underlying protocol and implementation work.

## Setup

Install Go 1.21+, then:

```bash
git clone https://github.com/nasif43/noirajjo
cd noirajjo
./build
```

Edit `yggdrasil.conf` to add local peers (other nodes running noirajjo or Yggdrasil on your ISP's network):

```json
{
  "Peers": [
    "tcp://[peer-ip]:12345"
  ]
}
```

Run:
```bash
./yggdrasil -useconffile yggdrasil.conf
```

Your node will get a stable IPv6 address and can reach any other Yggdrasil/noirajjo node it can find peers to.

## Original project

All credit for the underlying implementation goes to the [Yggdrasil Network team](https://github.com/yggdrasil-network/yggdrasil-go). This fork exists for a specific geographic and political context. If you're outside Bangladesh, the original repo is what you want.
