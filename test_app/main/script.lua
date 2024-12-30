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
            local counter = 1
            while true do
                stage = "loop" .. counter
                counter = counter + 1
                coroutine.yield(1000)
            end
        end)
    end
})
