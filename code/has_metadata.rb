module HasMetadata
  extend ActiveSupport::Concern

  included do |base|
    base.serialize :metadata, MetadataSerializer
    base.after_initialize :init_metadata
  end

  def init_metadata
    send("#{meta_attr_name}=", HashWithIndifferentAccess.new) unless send(:meta_attr_name)
  end

  def meta_attr_name
    :metadata
  end

  # stub, overridden when a meta_attr is defined.
  def meta_attrs
    { }
  end

  module ClassMethods
    def meta_attr_name(sym)
      encoders = serialize sym, MetadataSerializer
      encoders.delete "metadata"
      class_eval do
        define_method :meta_attr_name do
          sym
        end
      end
    end

    def meta_attrs
      @meta_attrs ||= { }
    end

    def meta_attr(sym, options = { })
      self.meta_attrs[sym] = options
      klass = self # capture eigenclass

      if options[:required]
        klass.validates_presence_of sym
      end

      class_eval do
        define_method :meta_attrs do
          klass.meta_attrs
        end
        define_method sym do
          value = send(meta_attr_name)[sym]
          value = options[:default] if value.nil? && options[:default]
          # handle legacy data
          if meta_attrs[sym][:integer] && value.to_s.integer?
            value.to_i
          else
            value
          end
        end
        define_method "#{sym}=" do |val|
          send :attribute_will_change!, sym.to_s
          val = val.to_i if meta_attrs[sym][:integer] && !val.nil?
          send(meta_attr_name)[sym] = val
        end
        define_method "has_#{sym}?" do
          true
        end
      end
    end

  end

  class MetadataSerializer < Hash
    def self.dump(meta)
      meta.to_json
    end

    # custom load method to prevent legacy data from shitting the bed.
    def self.load(str)
      parsed = JSON.parse(str) rescue { unsafe_metadata: str }
      parsed = parsed.is_a?(Hash) ? parsed : { unsafe_metadata: parsed }
      HashWithIndifferentAccess.new parsed
    end
  end

end
