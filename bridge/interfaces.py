from __future__ import annotations

from typing import Any, Callable, Mapping, Protocol

MessageCallback = Callable[[Any], None]
ErrorCallback = Callable[[str, Exception, Any], None]


class ZenohSubscriber(Protocol):
    """Abstracts the zenoh subscribe side."""

    def subscribe(self, topic: str, callback: MessageCallback) -> None: ...


class RosPublisher(Protocol):
    """Abstracts a ROS publisher handle."""

    def publish(self, message: Mapping[str, Any]) -> None: ...


class RosPublisherFactory(Protocol):
    """Creates ROS publishers without importing ROS2 in core logic."""

    def create_publisher(
        self,
        topic: str,
        message_type: str,
        *,
        qos_profile: str | None = None,
        queue_size: int | None = None,
    ) -> RosPublisher: ...
