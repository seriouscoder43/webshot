from __future__ import annotations

from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

from mitmproxy import ctx, exceptions, http
from mitmproxy.proxy import server_hooks

kDenylistTimeoutSec = 5.0
kLocalHosts = frozenset({"test-target", "asset.test-target"})
kLocalHttpAddress = ("127.0.0.1", 18080)
kLocalHttpsAddress = ("127.0.0.1", 18443)


class WebshotProxy:
    def load(self, loader) -> None:
        loader.add_option(
            name="webshot_mode",
            typespec=str,
            default="prodlike",
            help="webshot runtime mode",
        )
        loader.add_option(
            name="webshot_denylist_url",
            typespec=str,
            default="http://127.0.0.1:8080/v1/denylist/check",
            help="denylist authority URL",
        )

    def configure(self, updates) -> None:
        if "webshot_mode" in updates and ctx.options.webshot_mode not in {"dev", "prodlike"}:
            raise exceptions.OptionsError("webshot_mode must be dev or prodlike")
        if "webshot_denylist_url" in updates and ctx.options.webshot_denylist_url.strip() == "":
            raise exceptions.OptionsError("webshot_denylist_url must not be empty")

    def request(self, flow: http.HTTPFlow) -> None:
        self._enforce_denylist(flow)

    def server_connect(self, data: server_hooks.ServerConnectionHookData) -> None:
        if ctx.options.webshot_mode != "dev":
            return
        if data.server.address is None:
            return

        original_host, original_port = data.server.address
        if original_host not in kLocalHosts:
            return

        if original_port == 80:
            data.server.address = kLocalHttpAddress
            data.server.sni = None
            return

        if original_port == 443:
            data.server.address = kLocalHttpsAddress
            data.server.sni = original_host

    def _enforce_denylist(self, flow: http.HTTPFlow) -> None:
        request = Request(
            url=ctx.options.webshot_denylist_url,
            data=flow.request.url.encode("utf-8"),
            method="POST",
            headers={"Content-Type": "text/plain; charset=utf-8"},
        )
        try:
            with urlopen(request, timeout=kDenylistTimeoutSec) as response:
                status = response.getcode()
                body = response.read()
                content_type = response.headers.get("Content-Type")
        except HTTPError as error:
            status = error.code
            body = error.read()
            content_type = error.headers.get("Content-Type")
        except URLError as error:
            self._fail_closed(flow, f"denylist authority unavailable: {error.reason}")
            return
        except Exception as error:
            self._fail_closed(flow, f"denylist authority error: {error}")
            return

        if status == 204:
            return
        if status == 403:
            headers = {} if content_type is None else {"Content-Type": content_type}
            flow.response = http.Response.make(status, body, headers)
            return
        self._fail_closed(flow, f"denylist authority returned unexpected status {status}")

    def _fail_closed(self, flow: http.HTTPFlow, message: str) -> None:
        flow.response = http.Response.make(
            502,
            message.encode("utf-8"),
            {"Content-Type": "text/plain; charset=utf-8"},
        )


addons = [WebshotProxy()]
