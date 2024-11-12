local json = require("json")
local mqtt = require("mqtt")

stage = "init"
paramstring1 = "init"
paramnumber1 = "init"
paramboolean1 = "init"

addcomponent({
    id = "component1",
    name = "Test component 1",
    description = "The test component 1",
    params = {
        string1 = { type = "string", name = "String1", default = "default1" },
        number1 = { type = "number", default = 123 },
        boolean1 = { type = "boolean", default = true },
        string2 = { type = "string" },
        invalid = {}
    },
    start = function(params)
        stage = "start"
        paramstring1 = params.string1
        paramnumber1 = tostring(params.number1)
        paramboolean1 = tostring(params.boolean1)
        return coroutine.create(function()
            stage = "begin"
            local counter = 1
            while coroutine.yield(1000) do
                stage = "loop" .. counter
                counter = counter + 1
            end
            stage = "end"
        end)
    end
})

-- addcomponent({
--     id = "component2",
--     name = "Test component 1",
--     description = "The test component 1",
--     params = {
--         string1 = { type = "string", name = "String1", default = "default1" },
--         number1 = { type = "number", default = 123 },
--         boolean1 = { type = "boolean", default = true },
--         string2 = { type = "string" },
--         invalid = {}
--     },
--     start = function(parameters)
--         stage = "start" 
--         paramstring1 = parameters.string1
--         paramnumber1 = tostring(parameters.number1)
--         paramboolean1 = tostring(parameters.boolean1)
--         print("component1 init", json.encode(parameters))
--         return coroutine.create(function()
--             stage = "begin"
--             while coroutine.yield() do
--                 stage = "loop"
--             end
--             stage = "end"
--         end)
--     end
-- })

-- os.adddriver({
--     id = "driver2",
--     name = "Driver 2",
--     description = "The driver 2",
--     coroutine = coroutine.create(function(config)
--         print("coroutine2 init", json.format(config))

          -- local client = mqtt.client("mqtt://broker.hivemq.com:1883")

            -- print("gonna connect")
            -- repeat
            --     local success = client:connect()
            -- until success
            -- print("client connect")

            -- client:subscribe("topic874/+", function (topic, data)
            --     print("handle1", topic)
            -- end)

            -- client:subscribe("topic874/a", function (topic, data)
            --     print("handle2", topic)
            --     client:unsubscribe("topic874/a")
            -- end)

            -- client:unsubscribe("topic874/b")


--         while coroutine.yield(1000) do
--             print("coroutine2 loop")
--         end

--         print("coroutine2 end")
--     end)
-- })

-- this comment is needed