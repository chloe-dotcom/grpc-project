from xmlrpc.server import SimpleXMLRPCServer

def add(a, b):
    return a + b

def sub(a, b):
    return a - b

server = SimpleXMLRPCServer(("localhost", 8000))
server.register_function(add, "add")
server.register_function(sub, "subtract")
server.serve_forever()