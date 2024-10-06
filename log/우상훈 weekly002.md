## 기록

2024-10-7

- unique_ptr
  - `std::unique_ptr<T> upr = std::make_unique<T>()`;
  - unique_ptr은 복사 생성자, 복사 대입 연산자가 delete되어 있다.
  - 다른 unique_ptr에 대입하고 싶으면 std::move를 사용해서 소유권을 넘겨줘야 한다. 결국 항상 한 명만 소유할 수 있다.

- shared_ptr
  - `shared_ptr<T> spr(new T());`    
    `shared_ptr<T> spr = std::make_shared<T>();`
  - shared_ptr은 _Ptr_base 클래스를 상속받고 있다.    
  _Ptr_base 클래스는 template 매개변수로 받아온 element_type에 대한 포인터를 멤버를 가지고 있고, 또 _Ref_count_base에 대한 포인터를 멤버로 가지고 있다. _Ref_count_base 타입으로 reference count를 관리하는 방식인 것이다.
  - 위의 두 할당 방식은 미묘하게 다르다.    
    - 첫 번째는 T 타입의 메모리와 reference count 관련 메모리가 따로 생성된다.    
    `[T] [RefCountingBlock]`
    - 두 번째는 std::make_shared에서 template 매개변수로 받은 타입으로 _Ref_count_obj2 타입을 생성해서( new _Ref_count_obj2<_Ty>() ) 이를 사용한다.    
    _Ref_count_obj2는 _Ty 타입에 대한 메모리를 소유한다. 게다가 이 클래스는 _Ref_count_base를 상속 받고 있기 때문에, 내가 소유한 메모리를 가지고 있을 뿐만 아니라 reference count도 같이 관리하게 되는 것이다.    
    `[T | RefCountingBlock]`
  - _Ref_count_base는 두 정수 uses와 weaks로 reference count를 관리한다.    
  uses는 몇 개의 shared_ptr가 참조하고 있는지, weaks는 몇 개의 weak_ptr가 참조하고 있는지를 나타낸다.    
  uses가 0이 되면 element_type*가 delete된다. 하지만 weaks가 0이 아니면 RefCountingBlock은 여전히 남아있는다.

- weak_ptr
  - `weak_ptr<T> wpr = spr;`
  - weak_ptr은 shared_ptr의 uses( 생명 주기 )에 영향을 주지도 않고 받지도 않는다.
  - shared_ptr을 받아서 사용한다. 사용 방법은 두 가지가 있다.    
  `bool expired = wpr.expired();` -> wpr이 가리키는 shared_ptr이 이미 소멸되었는지 확인한다.    
  혹은    
  `shared_ptr<T> spr = wpr.lock();` -> lock()은 wpr이 가리키는 shared_ptr을 반환한다. 만약 이미 소멸되었다면 nullptr을 반환한다.    
  `if ( spr ){
    ...
  }`

## 노트