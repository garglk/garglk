/*
 *   test of TadsObject.createInstance 
 */

class Item: object
    construct(x)
    {
        value = x;
    }
    generator(x)
    {
        return self.createInstance(x);
    }
    value = nil
;

class BlueItem: Item
    color = 'blue'
;

class RedItem: Item
    color = 'red'
;

main(args)
{
    local x;
    
    x = RedItem.generator(1);
    "red item color = <<x.color>>, value = <<x.value>>\n";

    x = BlueItem.generator(2);
    "blue item color = <<x.color>>, value = <<x.value>>\n";
}

