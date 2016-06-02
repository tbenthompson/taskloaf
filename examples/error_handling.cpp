
template <typename OkT, typename ErrT>
struct Result {
    // For better performance, we might want to use a variant here,
    // but this is simple!
    std::unique_ptr<OkT> ok;
    std::unique_ptr<ErrT> err;
}

int main() {
    auto ctx = tl::launch_local(2);
    auto stream = make_stream(10);
    auto result = stream.then(
        [] (FList<int>& x) { return sum_stream(x, 0); }
    ).unwrap().get();
    std::cout << "Total: " << result << std::endl;
}
