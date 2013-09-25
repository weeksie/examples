module JsonModel
  extend ActiveSupport::Concern

  module ClassMethods
    def inherited(subclass)
      subclass.instance_variable_set "@json_only", json_only_values.dup
      subclass.instance_variable_set "@json_except", json_except_values.dup
      subclass.instance_variable_set "@json_methods", json_methods_values.dup
      super
    end
    def json_only_values
      @json_only || [ ]
    end
    def json_except_values
      @json_except || [ ]
    end
    def json_methods_values
      @json_methods || [ ]
    end
    def json_only(*attrs)
      @json_only ||= [ ]
      @json_only |= attrs
    end
    def json_except(*attrs)
      @json_except ||= [ ]
      @json_except |= attrs
    end

    def json_methods(*methods)
      @json_methods           ||= [ ]
      existing_mapped_methods   = @json_methods.last.is_a?(Hash) ? @json_methods.pop : { }
      mapped_methods            = methods.last.is_a?(Hash)       ? methods.pop       : { }

      if @json_methods.empty?
        @json_methods = methods
      else
        @json_methods |= methods
      end

      @json_methods.push existing_mapped_methods.merge(mapped_methods)
    end

  end

  def as_json(options = { })
    except      = self.class.json_except_values.map(&:to_s)
    only        = self.class.json_only_values.map(&:to_s)

    options[:except] ||= except
    options[:only]   ||= only unless only.empty?

    json        = super options
    method_args = self.class.json_methods_values.dup

    unless method_args.empty?
      methods = method_args.pop
      method_args.each { |method| methods[method] = method }

      methods.each do |key, method|
        key = key.to_s
        if method.respond_to? :call
          json[key] = method.call self
        else
          key.chop! if method.to_s.ends_with?("?") && method.to_s == key
          json[key] = send method
        end

        if json[key].respond_to? :as_json
          json[key] = json[key].as_json(options[key.to_sym] || { } )
        end
      end
    end
    json.with_indifferent_access
  end
end
