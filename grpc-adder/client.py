import grpc
import calculator_pb2
import calculator_pb2_grpc

def run():
    with grpc.insecure_channel("localhost:50051") as channel:
        stub = calculator_pb2_grpc.CalculatorStub(channel)
        response = stub.Add(calculator_pb2.AddRequest(a=2, b=3))
        print(response.result)   # prints 5

if __name__ == "__main__":
    run() 