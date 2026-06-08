import xmlrpc.client

# client-side stub
proxy = xmlrpc.client.ServerProxy("http://localhost:8000")
# print(proxy.add(10, 3))
print(proxy.subtract(10, 3))