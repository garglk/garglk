class Thing: object
    name = 'foo'
    owner = nil
;

Thing template 'name';

transient Thing template @owner;

Thing template;


