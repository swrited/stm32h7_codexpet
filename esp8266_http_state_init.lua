-- ESP8266 NodeMCU HTTP -> UART state bridge
-- Save this file to ESP8266 as init.lua after changing WIFI_SSID/WIFI_PASS.

local WIFI_SSID = "swrited"
local WIFI_PASS = "0212wmdQQ"

uart.setup(0, 9600, 8, 0, 1, 0)

local function send_state(state)
    state = string.upper(state or "")

    if state == "THINKING" or state == "THINK" then
        uart.write(0, "STATE:THINKING\r\n")
        return "THINKING"
    elseif state == "ANSWERING" or state == "ANSWER" then
        uart.write(0, "STATE:ANSWERING\r\n")
        return "ANSWERING"
    elseif state == "IDLE" then
        uart.write(0, "STATE:IDLE\r\n")
        return "IDLE"
    elseif state == "BUSY" then
        uart.write(0, "STATE:BUSY\r\n")
        return "BUSY"
    end

    return nil
end

local function get_query_value(payload)
    local v = string.match(payload, "[?&]value=([A-Za-z]+)")
    if v == nil then
        v = string.match(payload, "GET /([A-Za-z]+)")
    end
    return v
end

wifi.setmode(wifi.STATION)
wifi.sta.config(WIFI_SSID, WIFI_PASS)
wifi.sta.connect()

print("Connecting WiFi...")

tmr.alarm(0, 1000, 1, function()
    local ip = wifi.sta.getip()
    if ip == nil then
        print("WiFi waiting...")
    else
        tmr.stop(0)
        print("WiFi connected: " .. ip)

        local server = net.createServer(net.TCP)
        server:listen(80, function(conn)
            conn:on("receive", function(conn, payload)
                local req_state = get_query_value(payload)
                local sent_state = send_state(req_state)
                local body

                if sent_state then
                    body = "OK STATE:" .. sent_state .. "\n"
                else
                    body = "Use /state?value=thinking or /thinking /answering /idle /busy\n"
                end

                conn:send("HTTP/1.1 200 OK\r\n")
                conn:send("Content-Type: text/plain\r\n")
                conn:send("Connection: close\r\n")
                conn:send("Content-Length: " .. string.len(body) .. "\r\n\r\n")
                conn:send(body)
            end)
            conn:on("sent", function(conn)
                conn:close()
            end)
        end)

        print("HTTP server started")
        print("Try: http://" .. ip .. "/state?value=thinking")
    end
end)
